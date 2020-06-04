// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_H_
#define MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_H_

#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/cpp/application/lazy_interface_ptr.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/public/interfaces/application/application.mojom.h"
#include "mojo/services/content_handler/public/interfaces/content_handler.mojom.h"
#include "mojo/services/html_viewer/ax_provider_impl.h"
#include "mojo/services/navigation/public/interfaces/navigation.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/services/view_manager/public/cpp/view_manager_client_factory.h"
#include "mojo/services/view_manager/public/cpp/view_manager_delegate.h"
#include "mojo/services/view_manager/public/cpp/view_observer.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebViewClient.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class WebEncryptedMediaClientImpl;
}

namespace mojo {
class ViewManager;
class View;
}

namespace html_viewer {

class AxProviderImpl;
class WebLayerTreeViewImpl;
class WebMediaPlayerFactory;

// A view for a single HTML document.
class HTMLDocument : public blink::WebViewClient,
                     public blink::WebFrameClient,
                     public mojo::ViewManagerDelegate,
                     public mojo::ViewObserver,
                     public mojo::InterfaceFactory<mojo::AxProvider> {
 public:
  // Load a new HTMLDocument with |response|.
  //
  // |service_provider_request| should be used to implement a
  // ServiceProvider which exposes services to the connecting application.
  // Commonly, the connecting application is the ViewManager and it will
  // request ViewManagerClient.
  //
  // |shell| is the Shell connection for this mojo::Application.
  HTMLDocument(mojo::ServiceProviderPtr provider,
               mojo::URLResponsePtr response,
               mojo::Shell* shell,
               scoped_refptr<base::MessageLoopProxy> compositor_thread,
               WebMediaPlayerFactory* web_media_player_factory);
  virtual ~HTMLDocument();

 private:
  // WebViewClient methods:
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();

  // WebWidgetClient methods:
  virtual void initializeLayerTreeView();
  virtual blink::WebLayerTreeView* layerTreeView();

  // WebFrameClient methods:
  virtual blink::WebMediaPlayer* createMediaPlayer(
      blink::WebLocalFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client);
  virtual blink::WebMediaPlayer* createMediaPlayer(
      blink::WebLocalFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client,
      blink::WebContentDecryptionModule* initial_cdm);
  virtual blink::WebFrame* createChildFrame(blink::WebLocalFrame* parent,
                                            const blink::WebString& frameName);
  virtual void frameDetached(blink::WebFrame*);
  virtual blink::WebCookieJar* cookieJar(blink::WebLocalFrame* frame);
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      blink::WebLocalFrame* frame,
      blink::WebDataSource::ExtraData* data,
      const blink::WebURLRequest& request,
      blink::WebNavigationType nav_type,
      blink::WebNavigationPolicy default_policy,
      bool isRedirect);
  virtual void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                      const blink::WebString& source_name,
                                      unsigned source_line,
                                      const blink::WebString& stack_trace);
  virtual void didNavigateWithinPage(blink::WebLocalFrame* frame,
                                     const blink::WebHistoryItem& history_item,
                                     blink::WebHistoryCommitType commit_type);
  virtual blink::WebEncryptedMediaClient* encryptedMediaClient();

  // ViewManagerDelegate methods:
  void OnEmbed(
      mojo::View* root,
      mojo::ServiceProviderImpl* embedee_service_provider_impl,
      scoped_ptr<mojo::ServiceProvider> embedder_service_provider) override;
  void OnViewManagerDisconnected(mojo::ViewManager* view_manager) override;

  // ViewObserver methods:
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;
  void OnViewDestroyed(mojo::View* view) override;
  void OnViewInputEvent(mojo::View* view, const mojo::EventPtr& event) override;

  // InterfaceFactory<AxProvider>
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::AxProvider> request) override;

  void Load(mojo::URLResponsePtr response);

  mojo::URLResponsePtr response_;
  mojo::ServiceProviderImpl exported_services_;
  scoped_ptr<mojo::ServiceProvider> embedder_service_provider_;
  mojo::Shell* shell_;
  mojo::LazyInterfacePtr<mojo::NavigatorHost> navigator_host_;
  blink::WebView* web_view_;
  mojo::View* root_;
  mojo::ViewManagerClientFactory view_manager_client_factory_;
  scoped_ptr<WebLayerTreeViewImpl> web_layer_tree_view_impl_;
  scoped_refptr<base::MessageLoopProxy> compositor_thread_;
  WebMediaPlayerFactory* web_media_player_factory_;

  // EncryptedMediaClient attached to this frame; lazily initialized.
  media::WebEncryptedMediaClientImpl* web_encrypted_media_client_;

  // HTMLDocument owns these pointers.
  std::set<AxProviderImpl*> ax_provider_impls_;

  DISALLOW_COPY_AND_ASSIGN(HTMLDocument);
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_H_
