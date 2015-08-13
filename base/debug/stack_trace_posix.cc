// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <ostream>
#include <string>
#include <vector>

#if defined(__GLIBCXX__)
#include <cxxabi.h>
#endif
#if !defined(__UCLIBC__)
#include <execinfo.h>
#endif

#if defined(OS_MACOSX)
#include <AvailabilityMacros.h>
#endif

#include "base/basictypes.h"
#include "base/debug/debugger.h"
#include "base/debug/proc_maps_linux.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"

#if defined(USE_SYMBOLIZE)
#include "base/third_party/symbolize/symbolize.h"
#endif

namespace base {
namespace debug {

namespace {

volatile sig_atomic_t in_signal_handler = 0;

#if !defined(USE_SYMBOLIZE) && defined(__GLIBCXX__)
// The prefix used for mangled symbols, per the Itanium C++ ABI:
// http://www.codesourcery.com/cxx-abi/abi.html#mangling
const char kMangledSymbolPrefix[] = "_Z";

// Characters that can be used for symbols, generated by Ruby:
// (('a'..'z').to_a+('A'..'Z').to_a+('0'..'9').to_a + ['_']).join
const char kSymbolCharacters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
#endif  // !defined(USE_SYMBOLIZE) && defined(__GLIBCXX__)

#if !defined(USE_SYMBOLIZE)
// Demangles C++ symbols in the given text. Example:
//
// "out/Debug/base_unittests(_ZN10StackTraceC1Ev+0x20) [0x817778c]"
// =>
// "out/Debug/base_unittests(StackTrace::StackTrace()+0x20) [0x817778c]"
void DemangleSymbols(std::string* text) {
  // Note: code in this function is NOT async-signal safe (std::string uses
  // malloc internally).

#if defined(__GLIBCXX__) && !defined(__UCLIBC__)

  std::string::size_type search_from = 0;
  while (search_from < text->size()) {
    // Look for the start of a mangled symbol, from search_from.
    std::string::size_type mangled_start =
        text->find(kMangledSymbolPrefix, search_from);
    if (mangled_start == std::string::npos) {
      break;  // Mangled symbol not found.
    }

    // Look for the end of the mangled symbol.
    std::string::size_type mangled_end =
        text->find_first_not_of(kSymbolCharacters, mangled_start);
    if (mangled_end == std::string::npos) {
      mangled_end = text->size();
    }
    std::string mangled_symbol =
        text->substr(mangled_start, mangled_end - mangled_start);

    // Try to demangle the mangled symbol candidate.
    int status = 0;
    scoped_ptr<char, base::FreeDeleter> demangled_symbol(
        abi::__cxa_demangle(mangled_symbol.c_str(), NULL, 0, &status));
    if (status == 0) {  // Demangling is successful.
      // Remove the mangled symbol.
      text->erase(mangled_start, mangled_end - mangled_start);
      // Insert the demangled symbol.
      text->insert(mangled_start, demangled_symbol.get());
      // Next time, we'll start right after the demangled symbol we inserted.
      search_from = mangled_start + strlen(demangled_symbol.get());
    } else {
      // Failed to demangle.  Retry after the "_Z" we just found.
      search_from = mangled_start + 2;
    }
  }

#endif  // defined(__GLIBCXX__) && !defined(__UCLIBC__)
}
#endif  // !defined(USE_SYMBOLIZE)

class BacktraceOutputHandler {
 public:
  virtual void HandleOutput(const char* output) = 0;

 protected:
  virtual ~BacktraceOutputHandler() {}
};

void OutputPointer(void* pointer, BacktraceOutputHandler* handler) {
  // This should be more than enough to store a 64-bit number in hex:
  // 16 hex digits + 1 for null-terminator.
  char buf[17] = { '\0' };
  handler->HandleOutput("0x");
  internal::itoa_r(reinterpret_cast<intptr_t>(pointer),
                   buf, sizeof(buf), 16, 12);
  handler->HandleOutput(buf);
}

#if defined(USE_SYMBOLIZE)
void OutputFrameId(intptr_t frame_id, BacktraceOutputHandler* handler) {
  // Max unsigned 64-bit number in decimal has 20 digits (18446744073709551615).
  // Hence, 30 digits should be more than enough to represent it in decimal
  // (including the null-terminator).
  char buf[30] = { '\0' };
  handler->HandleOutput("#");
  internal::itoa_r(frame_id, buf, sizeof(buf), 10, 1);
  handler->HandleOutput(buf);
}
#endif  // defined(USE_SYMBOLIZE)

void ProcessBacktrace(void *const *trace,
                      size_t size,
                      BacktraceOutputHandler* handler) {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

#if defined(USE_SYMBOLIZE)
  for (size_t i = 0; i < size; ++i) {
    OutputFrameId(i, handler);
    handler->HandleOutput(" ");
    OutputPointer(trace[i], handler);
    handler->HandleOutput(" ");

    char buf[1024] = { '\0' };

    // Subtract by one as return address of function may be in the next
    // function when a function is annotated as noreturn.
    void* address = static_cast<char*>(trace[i]) - 1;
    if (google::Symbolize(address, buf, sizeof(buf)))
      handler->HandleOutput(buf);
    else
      handler->HandleOutput("<unknown>");

    handler->HandleOutput("\n");
  }
#elif !defined(__UCLIBC__)
  bool printed = false;

  // Below part is async-signal unsafe (uses malloc), so execute it only
  // when we are not executing the signal handler.
  if (in_signal_handler == 0) {
    scoped_ptr<char*, FreeDeleter>
        trace_symbols(backtrace_symbols(trace, size));
    if (trace_symbols.get()) {
      for (size_t i = 0; i < size; ++i) {
        std::string trace_symbol = trace_symbols.get()[i];
        DemangleSymbols(&trace_symbol);
        handler->HandleOutput(trace_symbol.c_str());
        handler->HandleOutput("\n");
      }

      printed = true;
    }
  }

  if (!printed) {
    for (size_t i = 0; i < size; ++i) {
      handler->HandleOutput(" [");
      OutputPointer(trace[i], handler);
      handler->HandleOutput("]\n");
    }
  }
#endif  // defined(USE_SYMBOLIZE)
}

void PrintToStderr(const char* output) {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.
  ignore_result(HANDLE_EINTR(write(STDERR_FILENO, output, strlen(output))));
}

void StackDumpSignalHandler(int signal, siginfo_t* info, void* void_context) {
  // NOTE: This code MUST be async-signal safe.
  // NO malloc or stdio is allowed here.

  // Record the fact that we are in the signal handler now, so that the rest
  // of StackTrace can behave in an async-signal-safe manner.
  in_signal_handler = 1;

  if (BeingDebugged())
    BreakDebugger();

  PrintToStderr("Received signal ");
  char buf[1024] = { 0 };
  internal::itoa_r(signal, buf, sizeof(buf), 10, 0);
  PrintToStderr(buf);
  if (signal == SIGBUS) {
    if (info->si_code == BUS_ADRALN)
      PrintToStderr(" BUS_ADRALN ");
    else if (info->si_code == BUS_ADRERR)
      PrintToStderr(" BUS_ADRERR ");
    else if (info->si_code == BUS_OBJERR)
      PrintToStderr(" BUS_OBJERR ");
    else
      PrintToStderr(" <unknown> ");
  } else if (signal == SIGFPE) {
    if (info->si_code == FPE_FLTDIV)
      PrintToStderr(" FPE_FLTDIV ");
    else if (info->si_code == FPE_FLTINV)
      PrintToStderr(" FPE_FLTINV ");
    else if (info->si_code == FPE_FLTOVF)
      PrintToStderr(" FPE_FLTOVF ");
    else if (info->si_code == FPE_FLTRES)
      PrintToStderr(" FPE_FLTRES ");
    else if (info->si_code == FPE_FLTSUB)
      PrintToStderr(" FPE_FLTSUB ");
    else if (info->si_code == FPE_FLTUND)
      PrintToStderr(" FPE_FLTUND ");
    else if (info->si_code == FPE_INTDIV)
      PrintToStderr(" FPE_INTDIV ");
    else if (info->si_code == FPE_INTOVF)
      PrintToStderr(" FPE_INTOVF ");
    else
      PrintToStderr(" <unknown> ");
  } else if (signal == SIGILL) {
    if (info->si_code == ILL_BADSTK)
      PrintToStderr(" ILL_BADSTK ");
    else if (info->si_code == ILL_COPROC)
      PrintToStderr(" ILL_COPROC ");
    else if (info->si_code == ILL_ILLOPN)
      PrintToStderr(" ILL_ILLOPN ");
    else if (info->si_code == ILL_ILLADR)
      PrintToStderr(" ILL_ILLADR ");
    else if (info->si_code == ILL_ILLTRP)
      PrintToStderr(" ILL_ILLTRP ");
    else if (info->si_code == ILL_PRVOPC)
      PrintToStderr(" ILL_PRVOPC ");
    else if (info->si_code == ILL_PRVREG)
      PrintToStderr(" ILL_PRVREG ");
    else
      PrintToStderr(" <unknown> ");
  } else if (signal == SIGSEGV) {
    if (info->si_code == SEGV_MAPERR)
      PrintToStderr(" SEGV_MAPERR ");
    else if (info->si_code == SEGV_ACCERR)
      PrintToStderr(" SEGV_ACCERR ");
    else
      PrintToStderr(" <unknown> ");
  }
  if (signal == SIGBUS || signal == SIGFPE ||
      signal == SIGILL || signal == SIGSEGV) {
    internal::itoa_r(reinterpret_cast<intptr_t>(info->si_addr),
                     buf, sizeof(buf), 16, 12);
    PrintToStderr(buf);
  }
  PrintToStderr("\n");

#if defined(CFI_ENFORCEMENT)
  if (signal == SIGILL && info->si_code == ILL_ILLOPN) {
    PrintToStderr(
        "CFI: Most likely a control flow integrity violation; for more "
        "information see:\n");
    PrintToStderr(
        "https://www.chromium.org/developers/testing/control-flow-integrity\n");
  }
#endif

  debug::StackTrace().Print();

#if defined(OS_LINUX)
#if ARCH_CPU_X86_FAMILY
  ucontext_t* context = reinterpret_cast<ucontext_t*>(void_context);
  const struct {
    const char* label;
    greg_t value;
  } registers[] = {
#if ARCH_CPU_32_BITS
    { "  gs: ", context->uc_mcontext.gregs[REG_GS] },
    { "  fs: ", context->uc_mcontext.gregs[REG_FS] },
    { "  es: ", context->uc_mcontext.gregs[REG_ES] },
    { "  ds: ", context->uc_mcontext.gregs[REG_DS] },
    { " edi: ", context->uc_mcontext.gregs[REG_EDI] },
    { " esi: ", context->uc_mcontext.gregs[REG_ESI] },
    { " ebp: ", context->uc_mcontext.gregs[REG_EBP] },
    { " esp: ", context->uc_mcontext.gregs[REG_ESP] },
    { " ebx: ", context->uc_mcontext.gregs[REG_EBX] },
    { " edx: ", context->uc_mcontext.gregs[REG_EDX] },
    { " ecx: ", context->uc_mcontext.gregs[REG_ECX] },
    { " eax: ", context->uc_mcontext.gregs[REG_EAX] },
    { " trp: ", context->uc_mcontext.gregs[REG_TRAPNO] },
    { " err: ", context->uc_mcontext.gregs[REG_ERR] },
    { "  ip: ", context->uc_mcontext.gregs[REG_EIP] },
    { "  cs: ", context->uc_mcontext.gregs[REG_CS] },
    { " efl: ", context->uc_mcontext.gregs[REG_EFL] },
    { " usp: ", context->uc_mcontext.gregs[REG_UESP] },
    { "  ss: ", context->uc_mcontext.gregs[REG_SS] },
#elif ARCH_CPU_64_BITS
    { "  r8: ", context->uc_mcontext.gregs[REG_R8] },
    { "  r9: ", context->uc_mcontext.gregs[REG_R9] },
    { " r10: ", context->uc_mcontext.gregs[REG_R10] },
    { " r11: ", context->uc_mcontext.gregs[REG_R11] },
    { " r12: ", context->uc_mcontext.gregs[REG_R12] },
    { " r13: ", context->uc_mcontext.gregs[REG_R13] },
    { " r14: ", context->uc_mcontext.gregs[REG_R14] },
    { " r15: ", context->uc_mcontext.gregs[REG_R15] },
    { "  di: ", context->uc_mcontext.gregs[REG_RDI] },
    { "  si: ", context->uc_mcontext.gregs[REG_RSI] },
    { "  bp: ", context->uc_mcontext.gregs[REG_RBP] },
    { "  bx: ", context->uc_mcontext.gregs[REG_RBX] },
    { "  dx: ", context->uc_mcontext.gregs[REG_RDX] },
    { "  ax: ", context->uc_mcontext.gregs[REG_RAX] },
    { "  cx: ", context->uc_mcontext.gregs[REG_RCX] },
    { "  sp: ", context->uc_mcontext.gregs[REG_RSP] },
    { "  ip: ", context->uc_mcontext.gregs[REG_RIP] },
    { " efl: ", context->uc_mcontext.gregs[REG_EFL] },
    { " cgf: ", context->uc_mcontext.gregs[REG_CSGSFS] },
    { " erf: ", context->uc_mcontext.gregs[REG_ERR] },
    { " trp: ", context->uc_mcontext.gregs[REG_TRAPNO] },
    { " msk: ", context->uc_mcontext.gregs[REG_OLDMASK] },
    { " cr2: ", context->uc_mcontext.gregs[REG_CR2] },
#endif
  };

#if ARCH_CPU_32_BITS
  const int kRegisterPadding = 8;
#elif ARCH_CPU_64_BITS
  const int kRegisterPadding = 16;
#endif

  for (size_t i = 0; i < arraysize(registers); i++) {
    PrintToStderr(registers[i].label);
    internal::itoa_r(registers[i].value, buf, sizeof(buf),
                     16, kRegisterPadding);
    PrintToStderr(buf);

    if ((i + 1) % 4 == 0)
      PrintToStderr("\n");
  }
  PrintToStderr("\n");
#endif
#elif defined(OS_MACOSX)
  // TODO(shess): Port to 64-bit, and ARM architecture (32 and 64-bit).
#if ARCH_CPU_X86_FAMILY && ARCH_CPU_32_BITS
  ucontext_t* context = reinterpret_cast<ucontext_t*>(void_context);
  size_t len;

  // NOTE: Even |snprintf()| is not on the approved list for signal
  // handlers, but buffered I/O is definitely not on the list due to
  // potential for |malloc()|.
  len = static_cast<size_t>(
      snprintf(buf, sizeof(buf),
               "ax: %x, bx: %x, cx: %x, dx: %x\n",
               context->uc_mcontext->__ss.__eax,
               context->uc_mcontext->__ss.__ebx,
               context->uc_mcontext->__ss.__ecx,
               context->uc_mcontext->__ss.__edx));
  write(STDERR_FILENO, buf, std::min(len, sizeof(buf) - 1));

  len = static_cast<size_t>(
      snprintf(buf, sizeof(buf),
               "di: %x, si: %x, bp: %x, sp: %x, ss: %x, flags: %x\n",
               context->uc_mcontext->__ss.__edi,
               context->uc_mcontext->__ss.__esi,
               context->uc_mcontext->__ss.__ebp,
               context->uc_mcontext->__ss.__esp,
               context->uc_mcontext->__ss.__ss,
               context->uc_mcontext->__ss.__eflags));
  write(STDERR_FILENO, buf, std::min(len, sizeof(buf) - 1));

  len = static_cast<size_t>(
      snprintf(buf, sizeof(buf),
               "ip: %x, cs: %x, ds: %x, es: %x, fs: %x, gs: %x\n",
               context->uc_mcontext->__ss.__eip,
               context->uc_mcontext->__ss.__cs,
               context->uc_mcontext->__ss.__ds,
               context->uc_mcontext->__ss.__es,
               context->uc_mcontext->__ss.__fs,
               context->uc_mcontext->__ss.__gs));
  write(STDERR_FILENO, buf, std::min(len, sizeof(buf) - 1));
#endif  // ARCH_CPU_32_BITS
#endif  // defined(OS_MACOSX)

  PrintToStderr("[end of stack trace]\n");

  _exit(1);
}

class PrintBacktraceOutputHandler : public BacktraceOutputHandler {
 public:
  PrintBacktraceOutputHandler() {}

  void HandleOutput(const char* output) override {
    // NOTE: This code MUST be async-signal safe (it's used by in-process
    // stack dumping signal handler). NO malloc or stdio is allowed here.
    PrintToStderr(output);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintBacktraceOutputHandler);
};

class StreamBacktraceOutputHandler : public BacktraceOutputHandler {
 public:
  explicit StreamBacktraceOutputHandler(std::ostream* os) : os_(os) {
  }

  void HandleOutput(const char* output) override { (*os_) << output; }

 private:
  std::ostream* os_;

  DISALLOW_COPY_AND_ASSIGN(StreamBacktraceOutputHandler);
};

void WarmUpBacktrace() {
  // Warm up stack trace infrastructure. It turns out that on the first
  // call glibc initializes some internal data structures using pthread_once,
  // and even backtrace() can call malloc(), leading to hangs.
  //
  // Example stack trace snippet (with tcmalloc):
  //
  // #8  0x0000000000a173b5 in tc_malloc
  //             at ./third_party/tcmalloc/chromium/src/debugallocation.cc:1161
  // #9  0x00007ffff7de7900 in _dl_map_object_deps at dl-deps.c:517
  // #10 0x00007ffff7ded8a9 in dl_open_worker at dl-open.c:262
  // #11 0x00007ffff7de9176 in _dl_catch_error at dl-error.c:178
  // #12 0x00007ffff7ded31a in _dl_open (file=0x7ffff625e298 "libgcc_s.so.1")
  //             at dl-open.c:639
  // #13 0x00007ffff6215602 in do_dlopen at dl-libc.c:89
  // #14 0x00007ffff7de9176 in _dl_catch_error at dl-error.c:178
  // #15 0x00007ffff62156c4 in dlerror_run at dl-libc.c:48
  // #16 __GI___libc_dlopen_mode at dl-libc.c:165
  // #17 0x00007ffff61ef8f5 in init
  //             at ../sysdeps/x86_64/../ia64/backtrace.c:53
  // #18 0x00007ffff6aad400 in pthread_once
  //             at ../nptl/sysdeps/unix/sysv/linux/x86_64/pthread_once.S:104
  // #19 0x00007ffff61efa14 in __GI___backtrace
  //             at ../sysdeps/x86_64/../ia64/backtrace.c:104
  // #20 0x0000000000752a54 in base::debug::StackTrace::StackTrace
  //             at base/debug/stack_trace_posix.cc:175
  // #21 0x00000000007a4ae5 in
  //             base::(anonymous namespace)::StackDumpSignalHandler
  //             at base/process_util_posix.cc:172
  // #22 <signal handler called>
  StackTrace stack_trace;
}

}  // namespace

#if defined(USE_SYMBOLIZE)

// class SandboxSymbolizeHelper.
//
// The purpose of this class is to prepare and install a "file open" callback
// needed by the stack trace symbolization code
// (base/third_party/symbolize/symbolize.h) so that it can function properly
// in a sandboxed process.  The caveat is that this class must be instantiated
// before the sandboxing is enabled so that it can get the chance to open all
// the object files that are loaded in the virtual address space of the current
// process.
class SandboxSymbolizeHelper {
 public:
  // Returns the singleton instance.
  static SandboxSymbolizeHelper* GetInstance() {
    return Singleton<SandboxSymbolizeHelper>::get();
  }

 private:
  friend struct DefaultSingletonTraits<SandboxSymbolizeHelper>;

  SandboxSymbolizeHelper()
      : is_initialized_(false) {
    Init();
  }

  ~SandboxSymbolizeHelper() {
    UnregisterCallback();
    CloseObjectFiles();
  }

  // Returns a O_RDONLY file descriptor for |file_path| if it was opened
  // sucessfully during the initialization.  The file is repositioned at
  // offset 0.
  // IMPORTANT: This function must be async-signal-safe because it can be
  // called from a signal handler (symbolizing stack frames for a crash).
  int GetFileDescriptor(const char* file_path) {
    int fd = -1;

#if !defined(OFFICIAL_BUILD)
    if (file_path) {
      // The assumption here is that iterating over std::map<std::string, int>
      // using a const_iterator does not allocate dynamic memory, hense it is
      // async-signal-safe.
      std::map<std::string, int>::const_iterator it;
      for (it = modules_.begin(); it != modules_.end(); ++it) {
        if (strcmp((it->first).c_str(), file_path) == 0) {
          // POSIX.1-2004 requires an implementation to guarantee that dup()
          // is async-signal-safe.
          fd = dup(it->second);
          break;
        }
      }
      // POSIX.1-2004 requires an implementation to guarantee that lseek()
      // is async-signal-safe.
      if (fd >= 0 && lseek(fd, 0, SEEK_SET) < 0) {
        // Failed to seek.
        fd = -1;
      }
    }
#endif  // !defined(OFFICIAL_BUILD)

    return fd;
  }

  // Searches for the object file (from /proc/self/maps) that contains
  // the specified pc.  If found, sets |start_address| to the start address
  // of where this object file is mapped in memory, sets the module base
  // address into |base_address|, copies the object file name into
  // |out_file_name|, and attempts to open the object file.  If the object
  // file is opened successfully, returns the file descriptor.  Otherwise,
  // returns -1.  |out_file_name_size| is the size of the file name buffer
  // (including the null terminator).
  // IMPORTANT: This function must be async-signal-safe because it can be
  // called from a signal handler (symbolizing stack frames for a crash).
  static int OpenObjectFileContainingPc(uint64_t pc, uint64_t& start_address,
                                        uint64_t& base_address, char* file_path,
                                        int file_path_size) {
    // This method can only be called after the singleton is instantiated.
    // This is ensured by the following facts:
    // * This is the only static method in this class, it is private, and
    //   the class has no friends (except for the DefaultSingletonTraits).
    //   The compiler guarantees that it can only be called after the
    //   singleton is instantiated.
    // * This method is used as a callback for the stack tracing code and
    //   the callback registration is done in the constructor, so logically
    //   it cannot be called before the singleton is created.
    SandboxSymbolizeHelper* instance = GetInstance();

    // The assumption here is that iterating over
    // std::vector<MappedMemoryRegion> using a const_iterator does not allocate
    // dynamic memory, hence it is async-signal-safe.
    std::vector<MappedMemoryRegion>::const_iterator it;
    bool is_first = true;
    for (it = instance->regions_.begin(); it != instance->regions_.end();
         ++it, is_first = false) {
      const MappedMemoryRegion& region = *it;
      if (region.start <= pc && pc < region.end) {
        start_address = region.start;
        // Don't subtract 'start_address' from the first entry:
        // * If a binary is compiled w/o -pie, then the first entry in
        //   process maps is likely the binary itself (all dynamic libs
        //   are mapped higher in address space). For such a binary,
        //   instruction offset in binary coincides with the actual
        //   instruction address in virtual memory (as code section
        //   is mapped to a fixed memory range).
        // * If a binary is compiled with -pie, all the modules are
        //   mapped high at address space (in particular, higher than
        //   shadow memory of the tool), so the module can't be the
        //   first entry.
        base_address = (is_first ? 0U : start_address) - region.offset;
        if (file_path && file_path_size > 0) {
          strncpy(file_path, region.path.c_str(), file_path_size);
          // Ensure null termination.
          file_path[file_path_size - 1] = '\0';
        }
        return instance->GetFileDescriptor(region.path.c_str());
      }
    }
    return -1;
  }

  // Parses /proc/self/maps in order to compile a list of all object file names
  // for the modules that are loaded in the current process.
  // Returns true on success.
  bool CacheMemoryRegions() {
    // Reads /proc/self/maps.
    std::string contents;
    if (!ReadProcMaps(&contents)) {
      LOG(ERROR) << "Failed to read /proc/self/maps";
      return false;
    }

    // Parses /proc/self/maps.
    if (!ParseProcMaps(contents, &regions_)) {
      LOG(ERROR) << "Failed to parse the contents of /proc/self/maps";
      return false;
    }

    is_initialized_ = true;
    return true;
  }

  // Opens all object files and caches their file descriptors.
  void OpenSymbolFiles() {
    // Pre-opening and caching the file descriptors of all loaded modules is
    // not safe for production builds.  Hence it is only done in non-official
    // builds.  For more details, take a look at: http://crbug.com/341966.
#if !defined(OFFICIAL_BUILD)
    // Open the object files for all read-only executable regions and cache
    // their file descriptors.
    std::vector<MappedMemoryRegion>::const_iterator it;
    for (it = regions_.begin(); it != regions_.end(); ++it) {
      const MappedMemoryRegion& region = *it;
      // Only interesed in read-only executable regions.
      if ((region.permissions & MappedMemoryRegion::READ) ==
              MappedMemoryRegion::READ &&
          (region.permissions & MappedMemoryRegion::WRITE) == 0 &&
          (region.permissions & MappedMemoryRegion::EXECUTE) ==
              MappedMemoryRegion::EXECUTE) {
        if (region.path.empty()) {
          // Skip regions with empty file names.
          continue;
        }
        if (region.path[0] == '[') {
          // Skip pseudo-paths, like [stack], [vdso], [heap], etc ...
          continue;
        }
        // Avoid duplicates.
        if (modules_.find(region.path) == modules_.end()) {
          int fd = open(region.path.c_str(), O_RDONLY | O_CLOEXEC);
          if (fd >= 0) {
            modules_.insert(std::make_pair(region.path, fd));
          } else {
            LOG(WARNING) << "Failed to open file: " << region.path
                         << "\n  Error: " << strerror(errno);
          }
        }
      }
    }
#endif  // !defined(OFFICIAL_BUILD)
  }

  // Initializes and installs the symbolization callback.
  void Init() {
    if (CacheMemoryRegions()) {
      OpenSymbolFiles();
      google::InstallSymbolizeOpenObjectFileCallback(
          &OpenObjectFileContainingPc);
    }
  }

  // Unregister symbolization callback.
  void UnregisterCallback() {
    if (is_initialized_) {
      google::InstallSymbolizeOpenObjectFileCallback(NULL);
      is_initialized_ = false;
    }
  }

  // Closes all file descriptors owned by this instance.
  void CloseObjectFiles() {
#if !defined(OFFICIAL_BUILD)
    std::map<std::string, int>::iterator it;
    for (it = modules_.begin(); it != modules_.end(); ++it) {
      int ret = IGNORE_EINTR(close(it->second));
      DCHECK(!ret);
      it->second = -1;
    }
    modules_.clear();
#endif  // !defined(OFFICIAL_BUILD)
  }

  // Set to true upon successful initialization.
  bool is_initialized_;

#if !defined(OFFICIAL_BUILD)
  // Mapping from file name to file descriptor.  Includes file descriptors
  // for all successfully opened object files and the file descriptor for
  // /proc/self/maps.  This code is not safe for production builds.
  std::map<std::string, int> modules_;
#endif  // !defined(OFFICIAL_BUILD)

  // Cache for the process memory regions.  Produced by parsing the contents
  // of /proc/self/maps cache.
  std::vector<MappedMemoryRegion> regions_;

  DISALLOW_COPY_AND_ASSIGN(SandboxSymbolizeHelper);
};
#endif  // USE_SYMBOLIZE

bool EnableInProcessStackDumpingForSandbox() {
#if defined(USE_SYMBOLIZE)
  SandboxSymbolizeHelper::GetInstance();
#endif  // USE_SYMBOLIZE

  return EnableInProcessStackDumping();
}

bool EnableInProcessStackDumping() {
  // When running in an application, our code typically expects SIGPIPE
  // to be ignored.  Therefore, when testing that same code, it should run
  // with SIGPIPE ignored as well.
  struct sigaction sigpipe_action;
  memset(&sigpipe_action, 0, sizeof(sigpipe_action));
  sigpipe_action.sa_handler = SIG_IGN;
  sigemptyset(&sigpipe_action.sa_mask);
  bool success = (sigaction(SIGPIPE, &sigpipe_action, NULL) == 0);

  // Avoid hangs during backtrace initialization, see above.
  WarmUpBacktrace();

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_flags = SA_RESETHAND | SA_SIGINFO;
  action.sa_sigaction = &StackDumpSignalHandler;
  sigemptyset(&action.sa_mask);

  success &= (sigaction(SIGILL, &action, NULL) == 0);
  success &= (sigaction(SIGABRT, &action, NULL) == 0);
  success &= (sigaction(SIGFPE, &action, NULL) == 0);
  success &= (sigaction(SIGBUS, &action, NULL) == 0);
  success &= (sigaction(SIGSEGV, &action, NULL) == 0);
// On Linux, SIGSYS is reserved by the kernel for seccomp-bpf sandboxing.
#if !defined(OS_LINUX)
  success &= (sigaction(SIGSYS, &action, NULL) == 0);
#endif  // !defined(OS_LINUX)

  return success;
}

StackTrace::StackTrace() {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

#if !defined(__UCLIBC__)
  // Though the backtrace API man page does not list any possible negative
  // return values, we take no chance.
  count_ = base::saturated_cast<size_t>(backtrace(trace_, arraysize(trace_)));
#else
  count_ = 0;
#endif
}

void StackTrace::Print() const {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

#if !defined(__UCLIBC__)
  PrintBacktraceOutputHandler handler;
  ProcessBacktrace(trace_, count_, &handler);
#endif
}

#if !defined(__UCLIBC__)
void StackTrace::OutputToStream(std::ostream* os) const {
  StreamBacktraceOutputHandler handler(os);
  ProcessBacktrace(trace_, count_, &handler);
}
#endif

namespace internal {

// NOTE: code from sandbox/linux/seccomp-bpf/demo.cc.
char *itoa_r(intptr_t i, char *buf, size_t sz, int base, size_t padding) {
  // Make sure we can write at least one NUL byte.
  size_t n = 1;
  if (n > sz)
    return NULL;

  if (base < 2 || base > 16) {
    buf[0] = '\000';
    return NULL;
  }

  char *start = buf;

  uintptr_t j = i;

  // Handle negative numbers (only for base 10).
  if (i < 0 && base == 10) {
    // This does "j = -i" while avoiding integer overflow.
    j = static_cast<uintptr_t>(-(i + 1)) + 1;

    // Make sure we can write the '-' character.
    if (++n > sz) {
      buf[0] = '\000';
      return NULL;
    }
    *start++ = '-';
  }

  // Loop until we have converted the entire number. Output at least one
  // character (i.e. '0').
  char *ptr = start;
  do {
    // Make sure there is still enough space left in our output buffer.
    if (++n > sz) {
      buf[0] = '\000';
      return NULL;
    }

    // Output the next digit.
    *ptr++ = "0123456789abcdef"[j % base];
    j /= base;

    if (padding > 0)
      padding--;
  } while (j > 0 || padding > 0);

  // Terminate the output with a NUL character.
  *ptr = '\000';

  // Conversion to ASCII actually resulted in the digits being in reverse
  // order. We can't easily generate them in forward order, as we can't tell
  // the number of characters needed until we are done converting.
  // So, now, we reverse the string (except for the possible "-" sign).
  while (--ptr > start) {
    char ch = *ptr;
    *ptr = *start;
    *start++ = ch;
  }
  return buf;
}

}  // namespace internal

}  // namespace debug
}  // namespace base
