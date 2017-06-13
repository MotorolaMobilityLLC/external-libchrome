// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_START_H_
#define IPC_IPC_MESSAGE_START_H_

// Used by IPC_BEGIN_MESSAGES so that each message class starts from a unique
// base.  Messages have unique IDs across channels in order for the IPC logging
// code to figure out the message class from its ID.
enum IPCMessageStart {
  AutomationMsgStart = 0,
  FrameMsgStart,
  PageMsgStart,
  ViewMsgStart,
  InputMsgStart,
  ProfileImportMsgStart,
  TestMsgStart,
  DevToolsMsgStart,
  WorkerMsgStart,
  NaClMsgStart,
  UtilityMsgStart,
  GpuChannelMsgStart,
  MediaMsgStart,
  ServiceMsgStart,
  PpapiMsgStart,
  FirefoxImporterUnittestMsgStart,
  FileUtilitiesMsgStart,
  DatabaseMsgStart,
  DOMStorageMsgStart,
  SpeechRecognitionMsgStart,
  SafeBrowsingMsgStart,
  P2PMsgStart,
  ResourceMsgStart,
  FileSystemMsgStart,
  ChildProcessMsgStart,
  ClipboardMsgStart,
  BlobMsgStart,
  AppCacheMsgStart,
  AudioMsgStart,
  MidiMsgStart,
  ChromeMsgStart,
  DragMsgStart,
  PrintMsgStart,
  SpellCheckMsgStart,
  ExtensionMsgStart,
  VideoCaptureMsgStart,
  QuotaMsgStart,
  TextInputClientMsgStart,
  ChromeUtilityMsgStart,
  MediaStreamMsgStart,
  ChromeBenchmarkingMsgStart,
  JavaBridgeMsgStart,
  GamepadMsgStart,
  ShellMsgStart,
  AccessibilityMsgStart,
  PrefetchMsgStart,
  PrerenderMsgStart,
  ChromotingMsgStart,
  BrowserPluginMsgStart,
  AndroidWebViewMsgStart,
  MetroViewerMsgStart,
  CCMsgStart,
  MediaPlayerMsgStart,
  TracingMsgStart,
  PeerConnectionTrackerMsgStart,
  VisitedLinkMsgStart,
  AppShimMsgStart,
  WebRtcLoggingMsgStart,
  TtsMsgStart,
  WebSocketMsgStart,
  NaClHostMsgStart,
  WebRTCIdentityMsgStart,
  PowerMonitorMsgStart,
  EncryptedMediaMsgStart,
  CacheStorageMsgStart,
  ServiceWorkerMsgStart,
  MessagePortMsgStart,
  EmbeddedWorkerMsgStart,
  EmbeddedWorkerContextMsgStart,
  CastMsgStart,
  CdmMsgStart,
  MediaStreamTrackMetricsHostMsgStart,
  ChromeExtensionMsgStart,
  PushMessagingMsgStart,
  GinJavaBridgeMsgStart,
  ChromeUtilityPrintingMsgStart,
  AecDumpMsgStart,
  OzoneGpuMsgStart,
  ChromeUtilityExtensionsMsgStart,
  PlatformNotificationMsgStart,
  PDFMsgStart,
  LayoutTestMsgStart,
  NetworkHintsMsgStart,
  BluetoothMsgStart,
  CastMediaMsgStart,
  AwMessagePortMsgStart,
  SyncCompositorMsgStart,
  ExtensionsGuestViewMsgStart,
  GuestViewMsgStart,
  // Note: CastCryptoMsgStart and CastChannelMsgStart reserved for Chromecast
  // internal code. Contact gunsch@ before changing/removing.
  CastCryptoMsgStart,
  CastChannelMsgStart,
  DataReductionProxyStart,
  ChromeAppBannerMsgStart,
  AttachmentBrokerMsgStart,
  RenderProcessMsgStart,
  PageLoadMetricsMsgStart,
  IPCTestMsgStart,
  ArcInstanceMsgStart,
  ArcInstanceHostMsgStart,
  DistillerMsgStart,
  ArcCameraMsgStart,
  DWriteFontProxyMsgStart,
  MediaPlayerDelegateMsgStart,
  SurfaceViewManagerMsgStart,
  ExtensionWorkerMsgStart,
  SubresourceFilterMsgStart,
  LastIPCMsgStart  // Must come last.
};

#endif  // IPC_IPC_MESSAGE_START_H_
