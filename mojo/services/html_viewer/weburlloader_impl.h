// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBURLLOADER_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBURLLOADER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/common/handle_watcher.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

namespace mojo {
class NetworkService;
}

namespace html_viewer {

// The concrete type of WebURLRequest::ExtraData.
class WebURLRequestExtraData : public blink::WebURLRequest::ExtraData {
 public:
  WebURLRequestExtraData();
  virtual ~WebURLRequestExtraData();

  mojo::URLResponsePtr synthetic_response;
};

class WebURLLoaderImpl : public blink::WebURLLoader {
 public:
  explicit WebURLLoaderImpl(mojo::NetworkService* network_service);

 private:
  virtual ~WebURLLoaderImpl();

  // blink::WebURLLoader methods:
  virtual void loadSynchronously(const blink::WebURLRequest& request,
                                 blink::WebURLResponse& response,
                                 blink::WebURLError& error,
                                 blink::WebData& data);
  virtual void loadAsynchronously(const blink::WebURLRequest&,
                                  blink::WebURLLoaderClient* client);
  virtual void cancel();
  virtual void setDefersLoading(bool defers_loading);

  void OnReceivedResponse(const blink::WebURLRequest& request,
                          mojo::URLResponsePtr response);
  void OnReceivedError(mojo::URLResponsePtr response);
  void OnReceivedRedirect(const blink::WebURLRequest& request,
                          mojo::URLResponsePtr response);
  void ReadMore();
  void WaitToReadMore();
  void OnResponseBodyStreamReady(MojoResult result);

  blink::WebURLLoaderClient* client_;
  GURL url_;
  mojo::URLLoaderPtr url_loader_;
  mojo::ScopedDataPipeConsumerHandle response_body_stream_;
  mojo::common::HandleWatcher handle_watcher_;

  base::WeakPtrFactory<WebURLLoaderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderImpl);
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBURLLOADER_IMPL_H_
