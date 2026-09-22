# B-Line handover — 22 September 2026

## Resumption status

The work below was resumed after the user's “continue with handover” request. The drawing/playback draft was stabilized and the three UI requests were implemented. The current build and native drawing/playback/audio tests pass; the movie encoder/player and the actual Export Movie path were exercised with temporary fixtures. See [EDITOR-PREVIEW-AND-EXPORT.md](EDITOR-PREVIEW-AND-EXPORT.md) for final behavior, verification, and limits. The historical stop-point sections below are retained as an audit trail, not the current completion status.

## Follow-up review: standalone toolbar and light table

Before starting this follow-up, the three prior implementation heads were pushed
and their remote refs verified: B-Line `63395e4`, plugandpaint `761b9cb`, and
TimeLineProject `ce7b700`. The latest work replaces text toolbar buttons with
matching Font Awesome icons and fixes light-table reference/background fading,
compositing overlaps, shared toolbar state and active-layer retention in the
camera/export composite. Source changes span B-Line and plugandpaint;
TimeLineProject requires no further changes. The shared drawing dependency is
plugandpaint commit `696a6f3` (pushed to its `github/master` remote). See the current behavior and test
record in [DRAWING_WORKSPACE.md](DRAWING_WORKSPACE.md) and
[EDITOR-PREVIEW-AND-EXPORT.md](EDITOR-PREVIEW-AND-EXPORT.md).

The handover priorities are addressed: File > Open Recent; standalone drawing
workspace; deferred secondary drawing updates; playback/timecode; Shot List
presentation/filter choices; narrow Layers/Stroke Attributes docks; and the
post-export movie player with reveal/copy-path actions. Remaining acceptance
limits are native stylus hardware, large production projects, long-duration A/V
drift and the untested Windows Explorer branch. Audio mixing remains first-track
only, as documented. These limits are not claims of completed validation.

## Follow-up: one playback viewport and measured FPS

Interactive playback now selects one visible editor by window focus, retains the
last selected visible view while another control/window has focus, and falls back
when that editor is hidden or minimized. Both hidden means no viewport rendering;
transport/audio continue. Pause/Stop restore all editing views. The detached
camera preview is suspended during playback. FPS is measured from distinct
painted frames and displayed against the target rate; timer wakeups align to
frame boundaries. See the current validation record in
[EDITOR-PREVIEW-AND-EXPORT.md](EDITOR-PREVIEW-AND-EXPORT.md).

Changes are in B-Line and the shared plugandpaint controller (`91a5566`). Tests exercise an
animated panel through the actual MainWindow transport and count paint events
on both widgets to verify inactive-view suppression, focus/visibility handoff,
resume behavior, and stable snapshots on expose requests.

## Stop point and user priorities

The user explicitly asked to **stop implementation and create a handover**. Implementation stopped. The changes below are unfinished, uncommitted, and not pushed. Do not interpret this document as evidence that the features work or that the current source builds.

The next session should carry forward both the existing drawing/playback work and these new requests:

1. Make the **Shot List less busy, more stylish, and intuitive**. Add a **Filter icon** with options for different views and detail levels.
2. Make **Layers and Stroke Attributes resizable to a substantially narrower width**. Setting only the dock minimum width is insufficient if its contents still impose a wide minimum size.
3. After successfully rendering a movie, open a **movie player window** with playback controls, **Show in Finder / Show in Explorer**, and **Copy Path**.

The earlier unfinished request is to make drawing smoother in both `PaintCanvas` and the integrated/legacy `PaintArea`, defer other drawing viewports until mouse/stylus release, repair confusing playback, and show readable sequence timecode in Camera View and the main UI.

No new Shot List, dock-layout, or movie-player implementation was started in this turn.

## Workspace and repository boundaries

Working directory: `/Users/andreascarlen/GameFusion/Applications/Boarder`.

Boarder compiles sibling application sources directly; changes must be handled per repository.

| Repository | Branch at handover | Last committed HEAD at handover | Current changes |
| --- | --- | --- | --- |
| Boarder / B-Line | `codex/level-editor-integration-20260825` | `c22e09b` — Add persistent Open Recent project list to File menu | MainWindow.cpp/.h, PaintCanvas.cpp, tests/PaintCanvasSmoke.cpp/.mk; new PlaybackTiming.h, tests/PlaybackSmoke.cpp, this document |
| ../plugandpaint | `master` | `9a1c2f8` — Share canvas rendering and editing with independent workspace views | app/Worker.cpp/.h, app/paintarea.cpp/.h |
| ../TimeLineProject | `main` | `80bd05c` — Add timeline AI review controls and stabilize track navigation | CursorItem.cpp, TimeLineView.h |

These HEADs and working-tree states were read locally during handover; no remote-ref audit was performed at this stop point. Earlier session context reported the first two committed changes pushed.

Known remotes, read at handover:

- B-Line `opensource` and `github`: `git@github.com:GameFusion/B-Line.git`. `origin` is empty.
- plugandpaint `github`: `git@github.com:GameFusion/plugandpaint.git`. Its `origin` still points at the old `stargit.gamefusion.io` address; do not use that legacy remote.
- TimeLineProject `origin`: `git@github.com:GameFusion/TimeLineProject.git`.
- The user's separate **GameFusion/GameEngine** routing requirement remains **Stargit .com/.dev, not GitHub and not .io**. Earlier verified canonical engine URL was `git@stargit.dev:/home/git/repositories/GameFusion.git`, hosted at `128.199.35.207`; reverify if needed. No engine files were edited in this turn. Do not create a replacement repository without checking the Stargit registry. Git-ref synchronization and website visibility are different claims.

Preserve unrelated untracked files:

- Boarder: `Doc_RV_EN.pdf`, `Screenshot_20251008_102147.png`, `docs/Bedtime Story.docx`, `docs/black.png`, `docs/getting-started-local-101-fr copy.pdf`, `rebase.sh`.
- plugandpaint: `plugins/.qtcreator/`, `plugins/build/`, `plugins/plugins.pro.user`.
- TimeLineProject: `build/`.

Do not stage these with the implementation. The IDE's active `BlueSpirit.md` belongs to a different operations task and is not a source of instructions for this work.

## Previously committed baseline

The standalone Drawing Workspace now uses an ordinary QWidget-backed QGraphicsView and shares the integrated PaintArea document/controller. It has independent zoom/pan, a QPicture drawing cache, mapped mouse/tablet events, existing editing tools, and a Camera preview. It replaced the incomplete prototype with the giant diagnostic diagonal. The integrated canvas remains the document source.

`File > Open Recent` was subsequently committed at `c22e09b`. It persists ten recent project directories via QSettings, records successful opens / Save As, disables missing entries, and offers Clear List.

See `docs/DRAWING_WORKSPACE.md` and the existing `tests/PaintCanvasSmoke.cpp` for the earlier workspace implementation and coverage. Earlier session context reported 29 checks passing before the present modifications; that is not validation of the current draft.

## Current drawing draft

Changed in `../plugandpaint/app/paintarea.cpp/.h`:

- Added `interactionActive()` / `workspaceInteractionActive()` and records which surface owns the left-button interaction.
- `PaintArea::update()` schedules the active drawing view during a held gesture; the release scope guard clears interaction state, recomposites, and updates both views.
- Suppresses layer/timeline thumbnail production during interaction and playback. Dirty layer thumbnail IDs are retained for later refresh.
- Freezes the camera preview during interaction. Added a shared preview cache keyed by the composite image cache key, camera rectangle, and transform so both views can reuse the same camera image.
- Draws the fitted in-progress curve directly in the legacy painter instead of allocating a full-canvas temporary image for every redraw.
- Does not invalidate the base layer raster for transform/opacity-only animation changes.
- Added `setPlaybackMode`, display text, and `drawPlaybackOverlay`. Legacy canvas and Camera View draw the overlay as UI; standalone and detached previews use the same helper. Export should remain clean, but the new export-isolation check has not run.

Changed in `../plugandpaint/app/Worker.cpp/.h`:

- Replaced a polling loop using `processEvents()` plus 1 ms sleep with a worker-thread 16 ms QTimer.
- Final completion is queued after the last submitted point; the UI no longer writes a worker-thread stop boolean directly.
- Disconnects a completed stroke's point input before the next stroke starts.
- Initializes WorkerResults indices and routes results by original layer UUID plus a stroke serial to prevent an old final result from clearing the newer stroke's preview.
- Moved expensive diagnostic fitting-error calculation behind the existing verbose condition.
- Important: the fitter still refits the whole collected stroke. This is coalescing, not incremental fitting; long-stroke performance still needs measurement.

Changed in `PaintCanvas.cpp`:

- Reuses the previous picture while the integrated view owns an interaction, and adds readable playback/timecode overlay to the camera preview or canvas when the preview is hidden.

Items to review before accepting the drawing draft:

- Held legacy and standalone mouse/stylus strokes; inactive viewport and PiP must remain still until release, then refresh with final fitted ink.
- Rapid consecutive strokes, release before the worker's first timer tick, layer selection changes, worker shutdown, and panel/project changes while a final fit is pending.
- The current worker callback drops results when its captured panel UUID differs from the displayed panel. Review this deliberately; a just-completed stroke must not silently disappear on navigation.
- Focus loss / cancelled gestures currently deserve special attention: interaction state primarily clears on left-button release.
- Check scene/cache invalidation, zoom, camera transforms, selection/erase and undo/redo. Make sure newly deferred thumbnails actually flush and are current rather than using a stale PiP image.
- `PaintArea::mousePressEvent` now has an empty/invalid-layer early return; verify this does not unintentionally prevent camera-only editing on an empty panel.
- `paintarea.cpp` also contains disabled historical Worker/fitter code. The active compiled Worker is `app/Worker.cpp`; do not edit the wrong implementation.

## Current playback/timecode draft

Primary integration points in `MainWindow.cpp`:

- Transport construction near lines 1600–1950.
- `onTimeCursorMoved` near line 6440.
- `startPlaybackAudio`, `updatePlaybackDisplay`, `play`, `pause`, `stop`, `onPlaybackTick` near lines 6924–7075.
- `refreshDetachedPip` and `onPaintAreaImageModified` near lines 7645–7690.

Draft behavior:

- A QElapsedTimer anchors sequence playback; audio silence or an unavailable device clock no longer prevents the visual cursor advancing.
- The frame time derives from elapsed time and project FPS, avoiding accumulation of timer-delay drift.
- Play starts at the cursor and resumes after Pause; Stop also resets after Pause; natural end holds the last frame; Loop wraps the clock and reseeks audio.
- Derives the playback extent from episode duration and actual timeline segments instead of the old arbitrary 10,000-second fallback.
- Scrubbing during playback reanchors the clock; timer-driven cursor updates do not reseek audio every frame.
- Adds Stop, Loop, and a timecode/state label beside transport controls. The label includes FPS; overlays show state and sequence timecode.
- `PlaybackTiming.h` formats non-drop-frame `HH:MM:SS:FF`, including the project `start_tc`. Project creation currently uses whole-number FPS. Fractional/drop-frame support is not established by this draft.
- Initializes the newly entered panel at the actual panel-local time, and takes the same-panel fast path during playback.
- Signals actual playback mode to PaintArea so playback no longer creates/saves timeline thumbnails on every animation frame.

Audio findings and limits:

- Previously a cursor scrub installed a stream, but Play did not. A newly opened project could have no attached stream or retain a stale one.
- `startPlaybackAudio()` now stops existing output, clears the server's public `_soundLayers`, selects the first nonempty Audio track's stream, seeks, releases frame mode, then plays.
- **This draft only attaches the first nonempty audio track. It does not implement multitrack mixing.** The macOS engine callback inspected in this session reads stream zero; do not claim full audio-track mixing is fixed.
- Review direct manipulation of `_soundLayers`, source stream lifetime, project replacement/deletion, and actual sound output. No audible playback test was performed.
- `TimeLineView.h` adds `setPlaybackActive` / `playbackActive`. `CursorItem::playSound` returns during transport playback so mouse scrubbing cannot replace the transport's audio stream after MainWindow has sought it.
- Loading another project and MainWindow destruction stop playback. Review other project/track deletion paths for equivalent stream-lifetime safety.
- Independent monotonic timing does not itself prove long-duration audio/video sync; measure audio start latency and drift with a real file.

UI review still needed:

- The existing transport has a fixed 400 px scale slider plus many buttons; the additional label/controls may crowd the row. Check real narrow window sizes and restructure if necessary.
- Verify labels remain readable over bright/dark content, PiP positioning, HiDPI, main-window and detached camera previews. Current preview overlay text is 12 logical pixels.
- Confirm state and timecode refresh when opening a project, changing FPS/start TC, moving the cursor, pausing, looping and reaching the end.

## Validation status at the stop

1. An intermediate app build completed successfully before later audio/interaction/cache/timeline changes.
2. The most recent build **failed** because `m_playbackActive` was initially inserted into another class in `TimeLineView.h` instead of `TimeLineView`.
3. That declaration was moved into `TimeLineView`'s private section (currently around line 271), but **no rebuild was run after that correction**.
4. The most recent build process has ended with exit code 2. No build is left running by this turn.
5. The current draft has **not** passed smoke tests or native visual checks. Do not call it buildable or ready to commit.
6. No current changes have been committed/pushed. No existing application process was deliberately quit/restarted, and no user project was opened/modified for these tests.

Build log: `/tmp/boarder-playback-build.log` (currently contains the failed build; subsequent runs overwrite it).

New/unrun test work:

- Extended `tests/PaintCanvasSmoke.cpp` with held-gesture deferral, rapid successive strokes, thumbnail suspension/flush, export overlay isolation, and timecode/frame-boundary checks.
- New `tests/PlaybackSmoke.cpp` instantiates the real MainWindow with isolated temporary QSettings and a synthetic timeline segment. Intended checks: empty timeline, silent monotonic playback, cursor start, pause/resume, stop-after-pause, natural end, loop, scrub reanchor, timecode label.
- Added a `PlaybackSmoke` make target to `tests/PaintCanvasSmoke.mk`.
- These additions were written immediately before the user stopped work and have not been compiled. Review fixture initialization and assumptions as well as product code.
- Intended screenshots: `/tmp/boarder-playback-workspace.png` and `/tmp/boarder-playback-transport.png`. They have not been generated by the new tests.

Resume commands (after reviewing the diff):

```sh
cd /Users/andreascarlen/GameFusion/Applications/Boarder/build-vs2019-qt6/build/Qt_6_10_2_for_macOS-Debug
make -j6 > /tmp/boarder-playback-build.log 2>&1
make -j6 -f ../../../tests/PaintCanvasSmoke.mk PaintCanvasSmoke PlaybackSmoke
QT_QPA_PLATFORM=offscreen ./PaintCanvasSmoke
QT_QPA_PLATFORM=offscreen ./PlaybackSmoke
```

Run native fixtures afterward if the offscreen checks pass, inspect the actual screenshots, then test real audio and interactions in an appropriate app instance. Do not overwrite/restart an existing user's session without preserving its state.

Qt qmake: `/Users/andreascarlen/Qt/6.10.2/macos/bin/qmake`. Build configuration: `build-vs2019-qt6/Boarder.pro`. Do not run `make` at the repository root; there is no root Makefile. Add new headers to the qmake project as appropriate and update test make dependencies.

Several source files have mixed CRLF/LF. Preserve unrelated line endings. For scoped whitespace checks use:

```sh
git -c core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol diff --check
```

Run independently in every changed repository. Build/test success and native visual/audio acceptance are separate evidence.

## Next UI work: Shot List

User intent: less visual noise, a more polished and intuitive hierarchy, plus a Filter icon for alternative views/detail levels.

Starting points:

- `BoarderMainWindow.ui`: `dockShots`, `shotsTreeWidget`.
- `MainWindow.cpp`: initial tree size policy near 2012, shot-tree connection near 2446, style near 2555, left dock grouping near 2735–2790, tree population and shot/panel selection handlers.
- Keep the existing Shot List distinct from the separate Shots panel and Perfect Script tabs.

Suggested approach, still a proposal:

- Start with a calm compact default showing essential shot identifier/name, thumbnail, and duration; move secondary metadata behind a Detailed mode.
- Add a labeled/tooltip-equipped filter icon menu with Compact / Thumbnails / Detailed choices, optional metadata visibility and hierarchy expand/collapse controls. Final exact modes can follow existing data and user workflows.
- Make selected shot/panel and scene hierarchy easy to read without many competing borders, icons or dense columns.
- Persist display preferences; changing a view/filter should not alter project content or lose the current selection.
- Validate long names, many shots/panels, narrow docks, dark theme, keyboard navigation and existing context-menu actions.

## Next UI work: narrow Layers / Stroke Attributes

Starting points:

- `MainWindow.cpp` around 2481 (layer list), 2549 (styles), 2605–2674 (StrokeAttributeDockWidget construction, horizontal split with Layers, max/min widths).
- `StrokeAttributeDockWidget.cpp/.h`: controls, layouts, label/value widths and preview sizing.
- `BoarderMainWindow.ui`: child widget/layout size constraints.

Both docks already receive `setMinimumWidth(0)`, and Stroke Attributes has maximum width 350; the user's problem persists. Inspect child minimumSizeHint, layout minimum constraints, fixed-width labels/sliders/preview and the side-by-side docking arrangement.

Suggested acceptance: drag each dock substantially narrower while controls stay operable; wrap/stack labels and values, allow sliders to shrink, use vertical scrolling when necessary, and avoid horizontal clipping. Test both docked and floating windows and saved/restored layout. A compact mode or tabbed arrangement is optional; do not substitute it for fixing minimum-width behavior.

## Next UI work: movie player after export

Entry point: `MainWindow::exportMovie()` currently around line 8487. Follow its successful final output/encoder completion path before adding the new window.

Required behavior:

- Open a player only after a movie has been successfully produced and its path exists.
- Use a normal independently resizable Qt player window, with play/pause, seek, elapsed/duration and volume controls.
- Show in Finder on macOS / Show in Explorer on Windows should reveal/select the rendered file, not just copy its folder.
- Copy Path should place the absolute file path on the clipboard and give unobtrusive feedback.
- Preserve export errors/cancellation. An unsupported playback codec should still leave Reveal and Copy Path usable.
- Prefer existing Qt Multimedia wiring if available; check the qmake dependencies before introducing QMediaPlayer/QVideoWidget. Do not assume that launching an external default application satisfies the requested player window.
- Verify spaces/non-ASCII characters in paths, repeated exports, playback-window lifetime and cross-platform reveal behavior. Use a small temporary rendered fixture, preserving real user output files.

## Completion and Git expectations for the next session

Resume from the actual dirty diffs, not from a clean checkout. First stabilize the current drawing/playback draft, then implement/review the requested UI improvements in sensible scoped steps. Keep incomplete work clearly identified throughout.

The earlier user authorized commit/push per project. At this stop point, preserve the unfinished state and this handover. Once subsequent work is validated, stage only intended paths, commit each affected repository separately, push its correct branch/remote, and verify the resulting remote ref. Report any unfinished native/audio acceptance honestly. Do not claim everything synchronized based only on a local commit.
