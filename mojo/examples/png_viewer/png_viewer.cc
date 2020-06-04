// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/strings/string_tokenizer.h"
#include "mojo/examples/media_viewer/media_viewer.mojom.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {
namespace examples {

class PNGViewer;

class ZoomableMediaImpl : public InterfaceImpl<ZoomableMedia> {
 public:
  ZoomableMediaImpl(ApplicationConnection* connection,
                    PNGViewer* viewer) : viewer_(viewer) {}
  virtual ~ZoomableMediaImpl() {}

 private:
  // Overridden from ZoomableMedia:
  virtual void ZoomIn() OVERRIDE;
  virtual void ZoomOut() OVERRIDE;
  virtual void ZoomToActualSize() OVERRIDE;

  PNGViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(ZoomableMediaImpl);
};

class NavigatorImpl : public InterfaceImpl<navigation::Navigator> {
 public:
  NavigatorImpl(ApplicationConnection* connection,
                PNGViewer* viewer) : viewer_(viewer) {}
  virtual ~NavigatorImpl() {}

 private:
  // Overridden from navigation::Navigate:
  virtual void Navigate(
      uint32_t node_id,
      navigation::NavigationDetailsPtr navigation_details,
      navigation::ResponseDetailsPtr response_details) OVERRIDE {
    int content_length = GetContentLength(response_details->response->headers);
    unsigned char* data = new unsigned char[content_length];
    unsigned char* buf = data;
    uint32_t bytes_remaining = content_length;
    uint32_t num_bytes = bytes_remaining;
    while (bytes_remaining > 0) {
      MojoResult result = ReadDataRaw(
          response_details->response_body_stream.get(),
          buf,
          &num_bytes,
          MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        Wait(response_details->response_body_stream.get(),
             MOJO_HANDLE_SIGNAL_READABLE,
             MOJO_DEADLINE_INDEFINITE);
      } else if (result == MOJO_RESULT_OK) {
        buf += num_bytes;
        num_bytes = bytes_remaining -= num_bytes;
      } else {
        break;
      }
    }

    SkBitmap bitmap;
    gfx::PNGCodec::Decode(static_cast<const unsigned char*>(data),
                          content_length, &bitmap);
    UpdateView(node_id, bitmap);

    delete[] data;
  }

  void UpdateView(view_manager::Id node_id, const SkBitmap& bitmap);

  int GetContentLength(const Array<String>& headers) {
    for (size_t i = 0; i < headers.size(); ++i) {
      base::StringTokenizer t(headers[i], ": ;=");
      while (t.GetNext()) {
        if (!t.token_is_delim() && t.token() == "Content-Length") {
          while (t.GetNext()) {
            if (!t.token_is_delim())
              return atoi(t.token().c_str());
          }
        }
      }
    }
    return 0;
  }

  PNGViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

class PNGViewer : public ApplicationDelegate,
                  public view_manager::ViewManagerDelegate {
 public:
  PNGViewer() : content_view_(NULL), zoom_percentage_(kDefaultZoomPercentage) {}
  virtual ~PNGViewer() {}

  void UpdateView(view_manager::Id node_id, const SkBitmap& bitmap) {
    bitmap_ = bitmap;
    zoom_percentage_ = kDefaultZoomPercentage;
    DrawBitmap();
  }

  void ZoomIn() {
    if (zoom_percentage_ >= kMaxZoomPercentage)
      return;
    zoom_percentage_ += kZoomStep;
    DrawBitmap();
  }

  void ZoomOut() {
    if (zoom_percentage_ <= kMinZoomPercentage)
      return;
    zoom_percentage_ -= kZoomStep;
    DrawBitmap();
  }

  void ZoomToActualSize() {
    if (zoom_percentage_ == kDefaultZoomPercentage)
      return;
    zoom_percentage_ = kDefaultZoomPercentage;
    DrawBitmap();
  }

 private:
  static const uint16_t kMaxZoomPercentage = 400;
  static const uint16_t kMinZoomPercentage = 20;
  static const uint16_t kDefaultZoomPercentage = 100;
  static const uint16_t kZoomStep = 20;

  // Overridden from ApplicationDelegate:
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->AddService<NavigatorImpl>(this);
    connection->AddService<ZoomableMediaImpl>(this);
    view_manager::ViewManager::ConfigureIncomingConnection(connection, this);
    return true;
  }

  // Overridden from view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) OVERRIDE {
    content_view_ = view_manager::View::Create(view_manager);
    root->SetActiveView(content_view_);
    content_view_->SetColor(SK_ColorRED);
    if (!bitmap_.isNull())
      DrawBitmap();
  }

  void DrawBitmap() {
    if (!content_view_)
      return;

    if (zoom_percentage_ == kDefaultZoomPercentage) {
      content_view_->SetContents(bitmap_);
      return;
    }

    skia::RefPtr<SkCanvas> canvas(skia::AdoptRef(skia::CreatePlatformCanvas(
        content_view_->node()->bounds().width(),
        content_view_->node()->bounds().height(),
        true)));
    canvas->drawColor(SK_ColorGRAY);
    SkPaint paint;
    SkScalar scale =
        SkFloatToScalar(zoom_percentage_ * 1.0f / kDefaultZoomPercentage);
    canvas->scale(scale, scale);
    canvas->drawBitmap(bitmap_, 0, 0, &paint);
    content_view_->SetContents(skia::GetTopDevice(*canvas)->accessBitmap(true));
  }

  view_manager::View* content_view_;
  SkBitmap bitmap_;
  uint16_t zoom_percentage_;

  DISALLOW_COPY_AND_ASSIGN(PNGViewer);
};

void ZoomableMediaImpl::ZoomIn() {
  viewer_->ZoomIn();
}

void ZoomableMediaImpl::ZoomOut() {
  viewer_->ZoomOut();
}

void ZoomableMediaImpl::ZoomToActualSize() {
  viewer_->ZoomToActualSize();
}

void NavigatorImpl::UpdateView(view_manager::Id node_id,
                               const SkBitmap& bitmap) {
  viewer_->UpdateView(node_id, bitmap);
}

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::PNGViewer;
}

}  // namespace mojo
