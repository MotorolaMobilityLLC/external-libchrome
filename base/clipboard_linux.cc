// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/clipboard.h"

#include <gtk/gtk.h>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/file_path.h"
#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "base/linux_util.h"
#include "base/string_util.h"

namespace {

const char kMimeBmp[] = "image/bmp";
const char kMimeHtml[] = "text/html";
const char kMimeText[] = "text/plain";
const char kMimeURI[] = "text/uri-list";
const char kMimeWebkitSmartPaste[] = "chromium/x-webkit-paste";

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

  std::string target_string = GdkAtomToString(selection_data->target);
  Clipboard::TargetMap::iterator iter = data_map->find(target_string);

  if (iter == data_map->end())
    return;

  if (target_string == kMimeBmp) {
    gtk_selection_data_set_pixbuf(selection_data,
        reinterpret_cast<GdkPixbuf*>(iter->second.first));
  } else if (target_string == kMimeURI) {
    gchar* uri_list[2];
    uri_list[0] = reinterpret_cast<gchar*>(iter->second.first);
    uri_list[1] = NULL;
    gtk_selection_data_set_uris(selection_data, uri_list);
  } else {
    gtk_selection_data_set(selection_data, selection_data->target, 8,
                           reinterpret_cast<guchar*>(iter->second.first),
                           iter->second.second);
  }
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
       iter != map->end(); ++iter) {
    if (iter->first == kMimeBmp)
      g_object_unref(reinterpret_cast<GdkPixbuf*>(iter->second.first));
    else
      ptrs.insert(iter->second.first);
  }

  for (std::set<char*>::iterator iter = ptrs.begin();
       iter != ptrs.end(); ++iter) {
    delete[] *iter;
  }

  delete map;
}

// Called on GdkPixbuf destruction; see WriteBitmap().
void GdkPixbufFree(guchar* pixels, gpointer data) {
  free(pixels);
}

}  // namespace

Clipboard::Clipboard() {
  clipboard_ = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  primary_selection_ = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
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
  scoped_array<GtkTargetEntry> targets(
      new GtkTargetEntry[clipboard_data_->size()]);

  int i = 0;
  for (Clipboard::TargetMap::iterator iter = clipboard_data_->begin();
       iter != clipboard_data_->end(); ++iter, ++i) {
    targets[i].target = static_cast<gchar*>(malloc(iter->first.size() + 1));
    memcpy(targets[i].target, iter->first.data(), iter->first.size());
    targets[i].target[iter->first.size()] = '\0';

    targets[i].flags = 0;
    targets[i].info = i;
  }

  gtk_clipboard_set_with_data(clipboard_, targets.get(),
                              clipboard_data_->size(),
                              GetData, ClearData,
                              clipboard_data_);

  for (size_t i = 0; i < clipboard_data_->size(); i++)
    free(targets[i].target);
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
  const gfx::Size* size = reinterpret_cast<const gfx::Size*>(size_data);

  guchar* data = base::BGRAToRGBA(reinterpret_cast<const uint8_t*>(pixel_data),
                                  size->width(), size->height(), 0);

  GdkPixbuf* pixbuf =
      gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, TRUE,
                               8, size->width(), size->height(),
                               size->width() * 4, GdkPixbufFree, NULL);
  // We store the GdkPixbuf*, and the size_t half of the pair is meaningless.
  // Note that this contrasts with the vast majority of entries in our target
  // map, which directly store the data and its length.
  InsertMapping(kMimeBmp, reinterpret_cast<char*>(pixbuf), 0);
}

void Clipboard::WriteBookmark(const char* title_data, size_t title_len,
                              const char* url_data, size_t url_len) {
  // Write as plain text.
  WriteText(url_data, url_len);

  // Write as a URI.
  char* data = new char[url_len + 1];
  memcpy(data, url_data, url_len);
  data[url_len] = NULL;
  InsertMapping(kMimeURI, data, url_len + 1);
}

void Clipboard::WriteFiles(const char* file_data, size_t file_len) {
  NOTIMPLEMENTED();
}

void Clipboard::WriteData(const char* format_name, size_t format_len,
                          const char* data_data, size_t data_len) {
  char* data = new char[data_len];
  memcpy(data, data_data, data_len);
  std::string format(format_name, format_len);
  InsertMapping(format.c_str(), data, data_len);
}

// We do not use gtk_clipboard_wait_is_target_available because of
// a bug with the gtk clipboard. It caches the available targets
// and does not always refresh the cache when it is appropriate.
bool Clipboard::IsFormatAvailable(const Clipboard::FormatType& format,
                                  Clipboard::Buffer buffer) const {
  GtkClipboard* clipboard = LookupBackingClipboard(buffer);
  if (clipboard == NULL)
    return false;

  bool format_is_plain_text = GetPlainTextFormatType() == format;
  if (format_is_plain_text) {
    // This tries a number of common text targets.
    if (gtk_clipboard_wait_is_text_available(clipboard))
      return true;
  }

  bool retval = false;
  GdkAtom* targets = NULL;
  GtkSelectionData* data =
      gtk_clipboard_wait_for_contents(clipboard,
                                      gdk_atom_intern("TARGETS", false));

  if (!data)
    return false;

  int num = 0;
  gtk_selection_data_get_targets(data, &targets, &num);

  // Some programs post data to the clipboard without any targets. If this is
  // the case we attempt to make sense of the contents as text. This is pretty
  // unfortunate since it means we have to actually copy the data to see if it
  // is available, but at least this path shouldn't be hit for conforming
  // programs.
  if (num <= 0) {
    if (format_is_plain_text) {
      gchar* text = gtk_clipboard_wait_for_text(clipboard);
      if (text) {
        g_free(text);
        retval = true;
      }
    }
  }

  GdkAtom format_atom = StringToGdkAtom(format);

  for (int i = 0; i < num; i++) {
    if (targets[i] == format_atom) {
      retval = true;
      break;
    }
  }

  g_free(targets);
  gtk_selection_data_free(data);

  return retval;
}

bool Clipboard::IsFormatAvailableByString(const std::string& format,
                                          Clipboard::Buffer buffer) const {
  return IsFormatAvailable(format, buffer);
}

void Clipboard::ReadText(Clipboard::Buffer buffer, string16* result) const {
  GtkClipboard* clipboard = LookupBackingClipboard(buffer);
  if (clipboard == NULL)
    return;

  result->clear();
  gchar* text = gtk_clipboard_wait_for_text(clipboard);

  if (text == NULL)
    return;

  // TODO(estade): do we want to handle the possible error here?
  UTF8ToUTF16(text, strlen(text), result);
  g_free(text);
}

void Clipboard::ReadAsciiText(Clipboard::Buffer buffer,
                              std::string* result) const {
  GtkClipboard* clipboard = LookupBackingClipboard(buffer);
  if (clipboard == NULL)
    return;

  result->clear();
  gchar* text = gtk_clipboard_wait_for_text(clipboard);

  if (text == NULL)
    return;

  result->assign(text);
  g_free(text);
}

void Clipboard::ReadFile(FilePath* file) const {
  *file = FilePath();
}

// TODO(estade): handle different charsets.
// TODO(port): set *src_url.
void Clipboard::ReadHTML(Clipboard::Buffer buffer, string16* markup,
                         std::string* src_url) const {
  GtkClipboard* clipboard = LookupBackingClipboard(buffer);
  if (clipboard == NULL)
    return;
  markup->clear();

  GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard,
      StringToGdkAtom(GetHtmlFormatType()));

  if (!data)
    return;

  UTF8ToUTF16(reinterpret_cast<char*>(data->data), data->length, markup);
  gtk_selection_data_free(data);
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
  // TODO(estade): implement this.
}

void Clipboard::ReadData(const std::string& format, std::string* result) {
  GtkSelectionData* data =
      gtk_clipboard_wait_for_contents(clipboard_, StringToGdkAtom(format));
  if (!data)
    return;
  result->assign(reinterpret_cast<char*>(data->data), data->length);
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

  if (iter != clipboard_data_->end()) {
    if (strcmp(kMimeBmp, key) == 0)
      g_object_unref(reinterpret_cast<GdkPixbuf*>(iter->second.first));
    else
      delete[] iter->second.first;
  }

  (*clipboard_data_)[key] = std::make_pair(data, data_len);
}

GtkClipboard* Clipboard::LookupBackingClipboard(Buffer clipboard) const {
  GtkClipboard* result;

  switch (clipboard) {
    case BUFFER_STANDARD:
      result = clipboard_;
      break;
    case BUFFER_SELECTION:
      result = primary_selection_;
      break;
    default:
      NOTREACHED();
      result = NULL;
      break;
  }
  return result;
}
