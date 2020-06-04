// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_manager.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/services/public/cpp/view_manager/lib/node_private.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_client_impl.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace view_manager {
namespace {

const char kWindowManagerURL[] = "mojo:window_manager";
const char kEmbeddedApp1URL[] = "mojo:embedded_app_1";

base::RunLoop* current_run_loop = NULL;

void DoRunLoop() {
  base::RunLoop run_loop;
  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = NULL;
}

void QuitRunLoop() {
  current_run_loop->Quit();
}

void WaitForAllChangesToBeAcked(ViewManagerClientImpl* client) {
  client->set_changes_acked_callback(base::Bind(&QuitRunLoop));
  DoRunLoop();
  client->ClearChangesAckedCallback();
}

class ConnectServiceLoader : public ServiceLoader,
                             public ApplicationDelegate,
                             public ViewManagerDelegate {
 public:
  typedef base::Callback<void(ViewManager*, Node*)> LoadedCallback;

  explicit ConnectServiceLoader(const LoadedCallback& callback)
      : callback_(callback) {
  }
  virtual ~ConnectServiceLoader() {}

 private:
  // Overridden from ServiceLoader:
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedMessagePipeHandle shell_handle) OVERRIDE {
    scoped_ptr<ApplicationImpl> app(new ApplicationImpl(this,
                                                        shell_handle.Pass()));
    apps_.push_back(app.release());
  }

  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE {
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      OVERRIDE {
    ViewManager::ConfigureIncomingConnection(connection, this);
    return true;
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnRootAdded(ViewManager* view_manager,
                           Node* root) OVERRIDE {
    callback_.Run(view_manager, root);
  }

  ScopedVector<ApplicationImpl> apps_;
  LoadedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ConnectServiceLoader);
};

class ActiveViewChangedObserver : public NodeObserver {
 public:
  explicit ActiveViewChangedObserver(Node* node)
      : node_(node) {}
  virtual ~ActiveViewChangedObserver() {}

 private:
  // Overridden from NodeObserver:
  virtual void OnNodeActiveViewChanged(Node* node,
                                       View* old_view,
                                       View* new_view) OVERRIDE {
    DCHECK_EQ(node, node_);
    QuitRunLoop();
  }

  Node* node_;

  DISALLOW_COPY_AND_ASSIGN(ActiveViewChangedObserver);
};

// Waits until the active view id of the supplied node changes.
void WaitForActiveViewToChange(Node* node) {
  ActiveViewChangedObserver observer(node);
  node->AddObserver(&observer);
  DoRunLoop();
  node->RemoveObserver(&observer);
}

class BoundsChangeObserver : public NodeObserver {
 public:
  explicit BoundsChangeObserver(Node* node) : node_(node) {}
  virtual ~BoundsChangeObserver() {}

 private:
  // Overridden from NodeObserver:
  virtual void OnNodeBoundsChanged(Node* node,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE {
    DCHECK_EQ(node, node_);
    QuitRunLoop();
  }

  Node* node_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangeObserver);
};

// Wait until the bounds of the supplied node change.
void WaitForBoundsToChange(Node* node) {
  BoundsChangeObserver observer(node);
  node->AddObserver(&observer);
  DoRunLoop();
  node->RemoveObserver(&observer);
}

// Spins a runloop until the tree beginning at |root| has |tree_size| nodes
// (including |root|).
class TreeSizeMatchesObserver : public NodeObserver {
 public:
  TreeSizeMatchesObserver(Node* tree, size_t tree_size)
      : tree_(tree),
        tree_size_(tree_size) {}
  virtual ~TreeSizeMatchesObserver() {}

  bool IsTreeCorrectSize() {
    return CountNodes(tree_) == tree_size_;
  }

 private:
  // Overridden from NodeObserver:
  virtual void OnTreeChanged(const TreeChangeParams& params) OVERRIDE {
    if (IsTreeCorrectSize())
      QuitRunLoop();
  }

  size_t CountNodes(const Node* node) const {
    size_t count = 1;
    Node::Children::const_iterator it = node->children().begin();
    for (; it != node->children().end(); ++it)
      count += CountNodes(*it);
    return count;
  }

  Node* tree_;
  size_t tree_size_;

  DISALLOW_COPY_AND_ASSIGN(TreeSizeMatchesObserver);
};

void WaitForTreeSizeToMatch(Node* node, size_t tree_size) {
  TreeSizeMatchesObserver observer(node, tree_size);
  if (observer.IsTreeCorrectSize())
    return;
  node->AddObserver(&observer);
  DoRunLoop();
  node->RemoveObserver(&observer);
}


// Utility class that waits for the destruction of some number of nodes and
// views.
class DestructionObserver : public NodeObserver, public ViewObserver {
 public:
  // |nodes| or |views| can be NULL.
  DestructionObserver(std::set<Id>* nodes, std::set<Id>* views)
      : nodes_(nodes),
        views_(views) {}

 private:
  // Overridden from NodeObserver:
  virtual void OnNodeDestroyed(Node* node) OVERRIDE {
    std::set<Id>::iterator it = nodes_->find(node->id());
    if (it != nodes_->end())
      nodes_->erase(it);
    if (CanQuit())
      QuitRunLoop();
  }

  // Overridden from ViewObserver:
  virtual void OnViewDestroyed(View* view) OVERRIDE {
    std::set<Id>::iterator it = views_->find(view->id());
    if (it != views_->end())
      views_->erase(it);
    if (CanQuit())
      QuitRunLoop();
  }

  bool CanQuit() {
    return (!nodes_ || nodes_->empty()) && (!views_ || views_->empty());
  }

  std::set<Id>* nodes_;
  std::set<Id>* views_;

  DISALLOW_COPY_AND_ASSIGN(DestructionObserver);
};

void WaitForDestruction(ViewManager* view_manager,
                        std::set<Id>* nodes,
                        std::set<Id>* views) {
  DestructionObserver observer(nodes, views);
  DCHECK(nodes || views);
  if (nodes) {
    for (std::set<Id>::const_iterator it = nodes->begin();
          it != nodes->end(); ++it) {
      view_manager->GetNodeById(*it)->AddObserver(&observer);
    }
  }
  if (views) {
    for (std::set<Id>::const_iterator it = views->begin();
          it != views->end(); ++it) {
      view_manager->GetViewById(*it)->AddObserver(&observer);
    }
  }
  DoRunLoop();
}

class OrderChangeObserver : public NodeObserver {
 public:
  OrderChangeObserver(Node* node) : node_(node) {
    node_->AddObserver(this);
  }
  virtual ~OrderChangeObserver() {
    node_->RemoveObserver(this);
  }

 private:
  // Overridden from NodeObserver:
  virtual void OnNodeReordered(Node* node,
                               Node* relative_node,
                               OrderDirection direction) OVERRIDE {
    DCHECK_EQ(node, node_);
    QuitRunLoop();
  }

  Node* node_;

  DISALLOW_COPY_AND_ASSIGN(OrderChangeObserver);
};

void WaitForOrderChange(ViewManager* view_manager, Node* node) {
  OrderChangeObserver observer(node);
  DoRunLoop();
}

// Tracks a node's destruction. Query is_valid() for current state.
class NodeTracker : public NodeObserver {
 public:
  explicit NodeTracker(Node* node) : node_(node) {
    node_->AddObserver(this);
  }
  virtual ~NodeTracker() {
    if (node_)
      node_->RemoveObserver(this);
  }

  bool is_valid() const { return !!node_; }

 private:
  // Overridden from NodeObserver:
  virtual void OnNodeDestroyed(Node* node) OVERRIDE {
    DCHECK_EQ(node, node_);
    node_ = NULL;
  }

  int id_;
  Node* node_;

  DISALLOW_COPY_AND_ASSIGN(NodeTracker);
};

}  // namespace

// ViewManager -----------------------------------------------------------------

// These tests model synchronization of two peer connections to the view manager
// service, that are given access to some root node.

class ViewManagerTest : public testing::Test {
 public:
  ViewManagerTest()
      : connect_loop_(NULL),
        loaded_view_manager_(NULL),
        window_manager_(NULL),
        commit_count_(0) {}

 protected:
  ViewManager* window_manager() { return window_manager_; }

  Node* CreateNodeInParent(Node* parent) {
    ViewManager* parent_manager = NodePrivate(parent).view_manager();
    Node* node = Node::Create(parent_manager);
    parent->AddChild(node);
    return node;
  }

  // Embeds another version of the test app @ node.
  ViewManager* Embed(ViewManager* view_manager, Node* node) {
    DCHECK_EQ(view_manager, NodePrivate(node).view_manager());
    node->Embed(kEmbeddedApp1URL);
    RunRunLoop();
    return GetLoadedViewManager();
  }

  // TODO(beng): remove these methods once all the tests are migrated.
  void DestroyViewManager1() {}
  ViewManager* view_manager_1() { return NULL; }
  ViewManager* view_manager_2() { return NULL; }

  ViewManager* GetLoadedViewManager() {
    ViewManager* view_manager = loaded_view_manager_;
    loaded_view_manager_ = NULL;
    return view_manager;
  }

  void UnloadApplication(const GURL& url) {
    test_helper_.SetLoaderForURL(scoped_ptr<ServiceLoader>(), url);
  }

 private:
  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    ConnectServiceLoader::LoadedCallback ready_callback =
        base::Bind(&ViewManagerTest::OnViewManagerLoaded,
                   base::Unretained(this));
    test_helper_.Init();
    test_helper_.SetLoaderForURL(
        scoped_ptr<ServiceLoader>(new ConnectServiceLoader(ready_callback)),
        GURL(kWindowManagerURL));
    test_helper_.SetLoaderForURL(
        scoped_ptr<ServiceLoader>(new ConnectServiceLoader(ready_callback)),
        GURL(kEmbeddedApp1URL));

    test_helper_.service_manager()->ConnectToService(
        GURL("mojo:mojo_view_manager"), &view_manager_init_);
    ASSERT_TRUE(EmbedRoot(view_manager_init_.get(), kWindowManagerURL));
  }

  void EmbedRootCallback(bool* result_cache, bool result) {
    *result_cache = result;
  }

  bool EmbedRoot(ViewManagerInitService* view_manager_init,
                 const std::string& url) {
    bool result = false;
    view_manager_init->EmbedRoot(
        url,
        base::Bind(&ViewManagerTest::EmbedRootCallback, base::Unretained(this),
                   &result));
    RunRunLoop();
    window_manager_ = GetLoadedViewManager();
    return result;
  }

  void OnViewManagerLoaded(ViewManager* view_manager, Node* root) {
    loaded_view_manager_ = view_manager;
    connect_loop_->Quit();
  }

  void RunRunLoop() {
    base::RunLoop run_loop;
    connect_loop_ = &run_loop;
    connect_loop_->Run();
    connect_loop_ = NULL;
  }

  base::MessageLoop loop_;
  base::RunLoop* connect_loop_;
  shell::ShellTestHelper test_helper_;
  ViewManagerInitServicePtr view_manager_init_;
  // Used to receive the most recent view manager loaded by an embed action.
  ViewManager* loaded_view_manager_;
  // The View Manager connection held by the window manager (app running at the
  // root node).
  ViewManager* window_manager_;
  int commit_count_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerTest);
};

TEST_F(ViewManagerTest, SetUp) {}

TEST_F(ViewManagerTest, Embed) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);
  EXPECT_TRUE(NULL != embedded);

  Node* node_in_embedded = embedded->GetRoots().front();
  EXPECT_EQ(node->parent(), window_manager()->GetRoots().front());
  EXPECT_EQ(NULL, node_in_embedded->parent());
}

// When Window Manager embeds A @ N, then creates N2 and parents to N, N becomes
// visible to A.
// TODO(beng): verify whether or not this is a policy we like.
TEST_F(ViewManagerTest, HierarchyChanged_NodeAdded) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);
  Node* nested = Node::Create(window_manager());
  node->AddChild(nested);
  WaitForTreeSizeToMatch(embedded->GetRoots().front(), 2);
  EXPECT_EQ(embedded->GetRoots().front()->children().front()->id(),
            nested->id());
}

// Window manager has two nodes, N1 & N2. Embeds A at N1. Creates node N21,
// a child of N2. Reparents N2 to N1. N1 should become visible to A.
// TODO(beng): verify whether or not this is a policy we like.
TEST_F(ViewManagerTest, HierarchyChanged_NodeMoved) {
  Node* node1 = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node1);
  ViewManager* embedded = Embed(window_manager(), node1);
  WaitForTreeSizeToMatch(embedded->GetRoots().front(), 1);

  Node* node2 = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node2);
  WaitForTreeSizeToMatch(embedded->GetRoots().front(), 1);
  EXPECT_TRUE(embedded->GetRoots().front()->children().empty());

  Node* node21 = Node::Create(window_manager());
  node2->AddChild(node21);
  WaitForTreeSizeToMatch(embedded->GetRoots().front(), 1);
  EXPECT_TRUE(embedded->GetRoots().front()->children().empty());

  // Makes node21 visible to |embedded|.
  node1->AddChild(node21);
  WaitForTreeSizeToMatch(embedded->GetRoots().front(), 2);
  EXPECT_FALSE(embedded->GetRoots().front()->children().empty());
  EXPECT_EQ(embedded->GetRoots().front()->children().front()->id(),
            node21->id());
}

// Window manager has two nodes, N1 and N11. Embeds A at N1. Removes N11 from
// N1. N11 should disappear from A.
// TODO(beng): verify whether or not this is a policy we like.
TEST_F(ViewManagerTest, HierarchyChanged_NodeRemoved) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  Node* nested = Node::Create(window_manager());
  node->AddChild(nested);

  ViewManager* embedded = Embed(window_manager(), node);
  EXPECT_EQ(embedded->GetRoots().front()->children().front()->id(),
            nested->id());

  node->RemoveChild(nested);
  WaitForTreeSizeToMatch(embedded->GetRoots().front(), 1);
  EXPECT_TRUE(embedded->GetRoots().front()->children().empty());
}

// Window manager has two nodes, N1 and N11. Embeds A at N1. Destroys N11.
// N11 should disappear from A.
// TODO(beng): verify whether or not this is a policy we like.
TEST_F(ViewManagerTest, NodeDestroyed) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  Node* nested = Node::Create(window_manager());
  node->AddChild(nested);

  ViewManager* embedded = Embed(window_manager(), node);
  EXPECT_EQ(embedded->GetRoots().front()->children().front()->id(),
            nested->id());

  // |nested| will be deleted after calling Destroy() below.
  Id id = nested->id();
  nested->Destroy();

  std::set<Id> nodes;
  nodes.insert(id);
  WaitForDestruction(embedded, &nodes, NULL);

  EXPECT_TRUE(embedded->GetRoots().front()->children().empty());
  EXPECT_EQ(NULL, embedded->GetNodeById(id));
}

TEST_F(ViewManagerTest, ViewManagerDestroyed_CleanupNode) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);

  Id node_id = node->id();

  UnloadApplication(GURL(kWindowManagerURL));

  std::set<Id> nodes;
  nodes.insert(node_id);
  WaitForDestruction(embedded, &nodes, NULL);

  EXPECT_TRUE(embedded->GetRoots().empty());
}

TEST_F(ViewManagerTest, SetActiveView) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);

  View* view = View::Create(window_manager());
  node->SetActiveView(view);

  Node* node_in_embedded = embedded->GetNodeById(node->id());
  WaitForActiveViewToChange(node_in_embedded);

  EXPECT_EQ(node_in_embedded->active_view()->id(), view->id());
}

TEST_F(ViewManagerTest, DestroyView) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);

  View* view = View::Create(window_manager());
  node->SetActiveView(view);

  Node* node_in_embedded = embedded->GetNodeById(node->id());
  WaitForActiveViewToChange(node_in_embedded);

  EXPECT_EQ(node_in_embedded->active_view()->id(), view->id());

  Id view_id = view->id();
  view->Destroy();

  std::set<Id> views;
  views.insert(view_id);
  WaitForDestruction(embedded, NULL, &views);
  EXPECT_EQ(NULL, node_in_embedded->active_view());
  EXPECT_EQ(NULL, embedded->GetViewById(view_id));
}

// Destroying the connection that created a node and view should result in that
// node and view disappearing from all connections that see them.
TEST_F(ViewManagerTest, ViewManagerDestroyed_CleanupNodeAndView) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  View* view = View::Create(window_manager());
  node->SetActiveView(view);
  ViewManager* embedded = Embed(window_manager(), node);

  Id node_id = node->id();
  Id view_id = view->id();

  UnloadApplication(GURL(kWindowManagerURL));

  std::set<Id> observed_nodes;
  observed_nodes.insert(node_id);
  std::set<Id> observed_views;
  observed_views.insert(view_id);
  WaitForDestruction(embedded, &observed_nodes, &observed_views);

  EXPECT_TRUE(embedded->GetRoots().empty());
  EXPECT_EQ(NULL, embedded->GetNodeById(node_id));
  EXPECT_EQ(NULL, embedded->GetViewById(view_id));
}

// This test validates the following scenario:
// -  a node originating from one connection
// -  a view originating from a second connection
// +  the connection originating the node is destroyed
// -> the view should still exist (since the second connection is live) but
//    should be disconnected from any nodes.
TEST_F(ViewManagerTest,
       ViewManagerDestroyed_CleanupNodeAndViewFromDifferentConnections) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);
  View* view_in_embedded = View::Create(embedded);
  Node* node_in_embedded = embedded->GetNodeById(node->id());
  node_in_embedded->SetActiveView(view_in_embedded);

  WaitForActiveViewToChange(node);

  Id node_id = node->id();
  Id view_id = view_in_embedded->id();

  UnloadApplication(GURL(kWindowManagerURL));
  std::set<Id> nodes;
  nodes.insert(node_id);
  WaitForDestruction(embedded, &nodes, NULL);

  EXPECT_TRUE(embedded->GetRoots().empty());
  // node was owned by the window manager, so it should be gone.
  EXPECT_EQ(NULL, embedded->GetNodeById(node_id));
  // view_in_embedded was owned by the embedded app, so it should still exist,
  // but disconnected from the node tree.
  EXPECT_EQ(view_in_embedded, embedded->GetViewById(view_id));
  EXPECT_EQ(NULL, view_in_embedded->node());
}

// This test verifies that it is not possible to set the active view to a view
// defined in a different connection.
// TODO(beng): write these tests for Node::AddChild(), RemoveChild() and
//             Contains().
TEST_F(ViewManagerTest, SetActiveViewAcrossConnection) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);

  View* view_in_embedded = View::Create(embedded);
  EXPECT_DEATH(node->SetActiveView(view_in_embedded), "");
}

// This test verifies that a node hierarchy constructed in one connection
// becomes entirely visible to the second connection when the hierarchy is
// attached.
TEST_F(ViewManagerTest, MapSubtreeOnAttach) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);

  // Create a subtree private to the window manager and make some changes to it.
  Node* child1 = Node::Create(window_manager());
  Node* child11 = Node::Create(window_manager());
  child1->AddChild(child11);
  gfx::Rect child11_bounds(800, 600);
  child11->SetBounds(child11_bounds);
  View* view11 = View::Create(window_manager());
  child11->SetActiveView(view11);
  WaitForAllChangesToBeAcked(
      static_cast<ViewManagerClientImpl*>(window_manager()));

  // When added to the shared node, the entire hierarchy and all property
  // changes should become visible to the embedded app.
  node->AddChild(child1);
  WaitForTreeSizeToMatch(embedded->GetRoots().front(), 3);

  Node* child11_in_embedded = embedded->GetNodeById(child11->id());
  View* view11_in_embedded = embedded->GetViewById(view11->id());
  EXPECT_TRUE(child11_in_embedded != NULL);
  EXPECT_EQ(view11_in_embedded, child11_in_embedded->active_view());
  EXPECT_EQ(child11_bounds, child11_in_embedded->bounds());
}

// Verifies that bounds changes applied to a node hierarchy in one connection
// are reflected to another.
TEST_F(ViewManagerTest, SetBounds) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);

  Node* node_in_embedded = embedded->GetNodeById(node->id());
  EXPECT_EQ(node->bounds(), node_in_embedded->bounds());

  node->SetBounds(gfx::Rect(100, 100));
  EXPECT_NE(node->bounds(), node_in_embedded->bounds());
  WaitForBoundsToChange(node_in_embedded);
  EXPECT_EQ(node->bounds(), node_in_embedded->bounds());
}

// Verifies that bounds changes applied to a node owned by a different
// connection are refused.
TEST_F(ViewManagerTest, SetBoundsSecurity) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);

  Node* node_in_embedded = embedded->GetNodeById(node->id());
  node->SetBounds(gfx::Rect(800, 600));
  WaitForBoundsToChange(node_in_embedded);

  node_in_embedded->SetBounds(gfx::Rect(1024, 768));
  // Bounds change should have been rejected.
  EXPECT_EQ(node->bounds(), node_in_embedded->bounds());
}

// Verifies that a node can only be destroyed by the connection that created it.
TEST_F(ViewManagerTest, DestroySecurity) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);

  Node* node_in_embedded = embedded->GetNodeById(node->id());

  NodeTracker tracker2(node_in_embedded);
  node_in_embedded->Destroy();
  // Node should not have been destroyed.
  EXPECT_TRUE(tracker2.is_valid());

  NodeTracker tracker1(node);
  node->Destroy();
  EXPECT_FALSE(tracker1.is_valid());
}

TEST_F(ViewManagerTest, MultiRoots) {
  Node* node1 = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node1);
  Node* node2 = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node2);
  ViewManager* embedded1 = Embed(window_manager(), node1);
  ViewManager* embedded2 = Embed(window_manager(), node2);
  EXPECT_EQ(embedded1, embedded2);
}

TEST_F(ViewManagerTest, EmbeddingIdentity) {
  Node* node = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node);
  ViewManager* embedded = Embed(window_manager(), node);
  EXPECT_EQ(kWindowManagerURL, embedded->GetEmbedderURL());
}

TEST_F(ViewManagerTest, Reorder) {
  Node* node1 = Node::Create(window_manager());
  window_manager()->GetRoots().front()->AddChild(node1);

  Node* node11 = Node::Create(window_manager());
  node1->AddChild(node11);
  Node* node12 = Node::Create(window_manager());
  node1->AddChild(node12);

  ViewManager* embedded = Embed(window_manager(), node1);

  Node* node1_in_embedded = embedded->GetNodeById(node1->id());

  {
    node11->MoveToFront();
    WaitForOrderChange(embedded, embedded->GetNodeById(node11->id()));

    EXPECT_EQ(node1_in_embedded->children().front(),
              embedded->GetNodeById(node12->id()));
    EXPECT_EQ(node1_in_embedded->children().back(),
              embedded->GetNodeById(node11->id()));
  }

  {
    node11->MoveToBack();
    WaitForOrderChange(embedded, embedded->GetNodeById(node11->id()));

    EXPECT_EQ(node1_in_embedded->children().front(),
              embedded->GetNodeById(node11->id()));
    EXPECT_EQ(node1_in_embedded->children().back(),
              embedded->GetNodeById(node12->id()));
  }
}

// TODO(beng): tests for view event dispatcher.
// - verify that we see events for all views.

// TODO(beng): tests for focus:
// - focus between two nodes known to a connection
// - focus between nodes unknown to one of the connections.
// - focus between nodes unknown to either connection.

}  // namespace view_manager
}  // namespace mojo
