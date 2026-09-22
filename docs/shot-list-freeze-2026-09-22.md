# Shot-list freeze after collapsing a scene

Collapsing SCENE_006 and selecting a shot in SCENE_008 left the Windows UI
unresponsive. Three live stack samples showed the UI thread in
`UIAutomationCore!UiaNodeTraverser::Traverse` / `BulkFetch::Traverse`, reached
from Qt's main event loop. The application shot-click handler was not on the
blocked stack. Stopping the inspection helper did not release the existing
in-process traversal.

Qt 6.10.2's tree collapse path removes visible rows without resetting the
accessibility child cache. A cached child can then map back to an earlier row,
causing Windows UI Automation's next-sibling navigation to revisit the same
elements indefinitely.

`ShotListPresentation` now emits a synchronous accessibility model-reset event
when a scene/shot branch collapses. It refreshes only the accessibility mapping;
project data, selection and expanded/collapsed state are retained.

Validation: the isolated native Qt regression test failed before the change
(`accessible child 26 maps back to 0` immediately after collapsing SCENE_006)
and passed after it. The test covers selecting SCENE_008, repeated collapse and
expand, and the expand-all/collapse-all commands. The Windows app was rebuilt;
the existing playback smoke suite also passed all 40 checks. All 75 runtime
files in the separate fixed bundle were verified against its updated manifest.

The user's frozen process (24732) was left open at their request, with unsaved
changes. A diagnostic dump was captured before the proposed recovery, which the
user declined. No debugger recovery or application termination was performed.
The separate fixed runtime is under
`build-vs2019-qt6/dist/windows/shot-list-fix/`.

The fix was subsequently included in the optimized Windows **1.0.2** release,
published to SyncPipeline Tools on September 22 at 16:17 UTC. All seven smoke
suites and fresh installer/runtime verification passed. See
[publication details](publishing-pipeline.md) for the archive, checksums and
catalog identifiers.

Reference: Qt's [tree implementation](https://github.com/qt/qtbase/blob/v6.10.2/src/widgets/itemviews/qtreeview.cpp)
and [Windows accessibility navigation](https://github.com/qt/qtbase/blob/v6.10.2/src/plugins/platforms/windows/uiautomation/qwindowsuiamainprovider.cpp).
