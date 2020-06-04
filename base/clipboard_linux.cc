// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/clipboard.h"
#include "base/string_util.h"

#include <gtk/gtk.h>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace {

static const char* kMimeHtml = "text/html";
static const char* kMimeText = "text/plain";
static const char* kMimeWebkitSmartPaste = "chrome-internal/webkit-paste";

std::string GdkAtomToString(const GdkAtom& atom) {
  gchar* name = gdk_atom_name(atom);
  std::string rv(name);
  g_free(name);
  return rv;
}

GdkAtom StringToGdkAtom(const std::string& str) {
  return gdk_atom_intern(str.c_str(), false);
}

// GtkClipboardGetFunc callback.
// GTK will call this when an application wants data we copied to the clipboard.
void GetData(GtkClipboard* clipboard,
             GtkSelectionData* selection_data,
             guint info,
             gpointer user_data) {
  Clipboard::TargetMap* data_map =
      reinterpret_cast<Clipboard::TargetMap*>(user_data);

  Clipboard::TargetMap::iterator iter =
      data_map->find(GdkAtomToString(selection_data->target));

  if (iter == data_map->end())
    return;

  gtk_selection_data_set(selection_data, selection_data->target, 8,
                         reinterpret_cast<guchar*>(iter->second.first),
                         iter->second.second);
}

// GtkClipboardClearFunc callback.
// We are guaranteed this will be called exactly once for each call to
// gtk_clipboard_set_with_data
void ClearData(GtkClipboard* clipboard,
               gpointer user_data) {
  Clipboard::TargetMap* map =
      reinterpret_cast<Clipboard::TargetMap*>(user_data);
  std::set<char*> ptrs;

  for (Clipboard::TargetMap::iterator iter = map->begin();
       iter != map->end(); ++iter)
    ptrs.insert(iter->second.first);

  for (std::set<char*>::iterator iter = ptrs.begin();
       iter != ptrs.end(); ++iter)
    delete[] *iter;

  delete map;
}

// Frees the pointers in the given map and clears the map.
// Does not double-free any pointers.
void FreeTargetMap(Clipboard::TargetMap map) {
  std::set<char*> ptrs;

  for (Clipboard::TargetMap::iterator iter = map.begin();
       iter != map.end(); ++iter)
    ptrs.insert(iter->second.first);

  for (std::set<char*>::iterator iter = ptrs.begin();
       iter != ptrs.end(); ++iter)
    delete[] *iter;

  map.clear();
}

}  // namespace

Clipboard::Clipboard() {
  clipboard_ = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
}

Clipboard::~Clipboard() {
  // TODO(estade): do we want to save clipboard data after we exit?
  // gtk_clipboard_set_can_store and gtk_clipboard_store work
  // but have strangely awful performance.
}

void Clipboard::WriteObjects(const ObjectMap& objects) {
  clipboard_data_ = new TargetMap();

  for (ObjectMap::const_iterator iter = objects.begin();
       iter != objects.end(); ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }

  SetGtkClipboard();
}

// Take ownership of the GTK clipboard and inform it of the targets we support.
void Clipboard::SetGtkClipboard() {
  GtkTargetEntry targets[clipboard_data_->size()];

  int i = 0;
  for (Clipboard::TargetMap::iterator iter = clipboard_data_->begin();
       iter != clipboard_data_->end(); ++iter, ++i) {
    char* target_string = new char[iter->first.size() + 1];
    strcpy(target_string, iter->first.c_str());
    targets[i].target = target_string;
    targets[i].flags = 0;
    targets[i].info = i;
  }

  gtk_clipboard_set_with_data(clipboard_, targets,
                              clipboard_data_->size(),
                              GetData, ClearData,
                              clipboard_data_);

  for (size_t i = 0; i < clipboard_data_->size(); i++)
    delete[] targets[i].target;
}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  char* data = new char[text_len];
  memcpy(data, text_data, text_len);

  InsertMapping(kMimeText, data, text_len);
  InsertMapping("TEXT", data, text_len);
  InsertMapping("STRING", data, text_len);
  InsertMapping("UTF8_STRING", data, text_len);
  InsertMapping("COMPOUND_TEXT", data, text_len);
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
  // TODO(estade): might not want to ignore |url_data|
  char* data = new char[markup_len];
  memcpy(data, markup_data, markup_len);

  InsertMapping(kMimeHtml, data, markup_len);
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void Clipboard::WriteWebSmartPaste() {
  InsertMapping(kMimeWebkitSmartPaste, NULL, 0);
}

void Clipboard::WriteBitmap(const char* pixel_data, const char* size_data) {
  NOTIMPLEMENTED();
}

void Clipboard::WriteBookmark(const char* title_data, size_t title_len,
                              const char* url_data, size_t url_len) {
  // TODO(estade): implement this, but for now fail silently so we do not
  // write error output during layout tests.
  // NOTIMPLEMENTED();
}

void Clipboard::WriteHyperlink(const char* title_data, size_t title_len,
                               const char* url_data, size_t url_len) {
  NOTIMPLEMENTED();
}

void Clipboard::WriteFiles(const char* file_data, size_t file_len) {
  NOTIMPLEMENTED();
}

// We do not use gtk_clipboard_wait_is_target_available because of
// a bug with the gtk clipboard. It caches the available targets
// and does not always refresh the cache when it is appropriate.
// TODO(estade): When gnome bug 557315 is resolved, change this function
// to use gtk_clipboard_wait_is_target_available. Also, catch requests
// for plain text and change them to gtk_clipboard_wait_is_text_available
// (which checks for several standard text targets).
bool Clipboard::IsFormatAvailable(const Clipboard::FormatType& format) const {
  bool retval = false;
  GdkAtom* targets = NULL;
  GtkSelectionData* data =
      gtk_clipboard_wait_for_contents(clipboard_,
                                      gdk_atom_intern("TARGETS", false));

  if (!data)
    return false;

  int num = 0;
  gtk_selection_data_get_targets(data, &targets, &num);

  GdkAtom format_atom = StringToGdkAtom(format);

  for (int i = 0; i < num; i++) {
    if (targets[i] == format_atom) {
      retval = true;
      break;
    }
  }

  gtk_selection_data_free(data);
  g_free(targets);

  return retval;
}

void Clipboard::ReadText(string16* result) const {
  result->clear();
  gchar* text = gtk_clipboard_wait_for_text(clipboard_);

  if (text == NULL)
   return;

  // TODO(estade): do we want to handle the possible error here?
  UTF8ToUTF16(text, strlen(text), result);
  g_free(text);
}

void Clipboard::ReadAsciiText(std::string* result) const {
  result->clear();
  gchar* text = gtk_clipboard_wait_for_text(clipboard_);

  if (text == NULL)
   return;

  result->assign(text);
  g_free(text);
}

void Clipboard::ReadFile(FilePath* file) const {
  *file = FilePath();
}

// TODO(estade): handle different charsets.
void Clipboard::ReadHTML(string16* markup, std::string* src_url) const {
  markup->clear();

  GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard_,
      StringToGdkAtom(GetHtmlFormatType()));

  if (!data)
    return;

  UTF8ToUTF16(reinterpret_cast<char*>(data->data),
              strlen(reinterpret_cast<char*>(data->data)),
              markup);
  gtk_selection_data_free(data);
}

// static
Clipboard::FormatType Clipboard::GetPlainTextFormatType() {
  return GdkAtomToString(GDK_TARGET_STRING);
}

// static
Clipboard::FormatType Clipboard::GetPlainTextWFormatType() {
  return GetPlainTextFormatType();
}

// static
Clipboard::FormatType Clipboard::GetHtmlFormatType() {
  return std::string(kMimeHtml);
}

// static
Clipboard::FormatType Clipboard::GetWebKitSmartPasteFormatType() {
  return std::string(kMimeWebkitSmartPaste);
}

// Insert the key/value pair in the clipboard_data structure. If
// the mapping already exists, it frees the associated data. Don't worry
// about double freeing because if the same key is inserted into the
// map twice, it must have come from different Write* functions and the
// data pointer cannot be the same.
void Clipboard::InsertMapping(const char* key,
                              char* data,
                              size_t data_len) {
  TargetMap::iterator iter = clipboard_data_->find(key);

  if (iter != clipboard_data_->end())
    delete[] iter->second.first;

  (*clipboard_data_)[key] = std::make_pair(data, data_len);
}
