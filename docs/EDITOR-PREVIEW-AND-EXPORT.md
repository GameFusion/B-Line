# Drawing, transport and editor review

The integrated PaintArea and standalone Drawing Workspace share the document. While a mouse or stylus gesture is held, redraw requests go to the active canvas; the other canvas, camera preview and thumbnails refresh after release. Stroke fitting is coalesced on a 16 ms worker timer, and the integrated canvas draws the in-progress fitted curve directly. Pending fits are committed at save/project-change boundaries. A late result following panel navigation is routed to its original panel through the normal undo path.

The standalone toolbar now uses the same Font Awesome tool glyphs and selected-state colors as the embedded editor, plus icon controls for navigation, camera preview and history. Tool selection and light-table state stay synchronized between windows. Light table now fades the panel reference and the composed non-active layers once, preserving overlaps and blend modes, then draws the active layer with its authored visibility, transform and opacity. The full composite remains intact for previews, thumbnails and export.

Playback uses elapsed time and the project frame rate, so silent sequences advance too. Play starts at the cursor, Pause holds it, Stop returns to the start, and natural completion holds the last frame. Loop and seeks update both the visual clock and audio. Sequence timecode (`HH:MM:SS:FF`) includes the project start TC and appears beside transport controls and over the canvas/camera previews. It is a UI overlay and is not burned into exported frames. The current timecode formatter is non-drop-frame for the project's whole-number FPS options.

The Shot List filter button offers Compact, Thumbnails and Detailed modes, panel/dialogue visibility, optional detailed columns, and Expand/Collapse All. Search matches metadata and retains the parent hierarchy. Display preferences persist in the existing B-Line QSettings. Clicking a shot opens its first panel. Panels and dialogue use the appropriate metadata columns; UUIDs remain internal. Thumbnail images are refreshed after editing.

Layers and Stroke Attributes use smaller thumbnails, shrinkable controls and a wrapping swatch grid. Both were exercised at 200 px width in native Qt windows. Stroke Attributes remains vertically scrollable when the available height is small.

Export Movie now encodes the rendered frames to an H.264 MP4 and opens an independent Qt movie player. The player supports play/pause, seeking, volume, Show in Finder/Explorer and Copy Path. Export errors/cancellation do not open a player; rendered frames are retained. FFmpeg must be on PATH, beside the executable, or in the supported Homebrew locations. The current export retains the existing 1920 × 1080 frame size.

Audio playback and export retain the existing first-track behavior; multitrack mixing is not implemented by this change. Native audio queue startup, consumption and Pause were verified with a temporary stereo WAV; this is not a long-duration A/V drift certification. macOS Finder reveal was exercised with a filename containing spaces and a non-ASCII character. The Windows reveal branch has not been run on Windows.

## Validation

Run from `build-vs2019-qt6/build/Qt_6_10_2_for_macOS-Debug` with its generated Makefile:

```sh
make -j6 -f ../../../tests/PaintCanvasSmoke.mk all PaintCanvasSmoke PlaybackSmoke MoviePlayerSmoke ExportMovieSmoke
QT_QPA_PLATFORM=offscreen ./PaintCanvasSmoke
QT_QPA_PLATFORM=offscreen ./PlaybackSmoke
QT_QPA_PLATFORM=offscreen ./MoviePlayerSmoke
QT_QPA_PLATFORM=offscreen ./ExportMovieSmoke
```

Native checks:

```sh
./PaintCanvasSmoke
./PlaybackSmoke --audio
./MoviePlayerSmoke
```

Verified during this change:

- PaintCanvasSmoke: 66 checks, including both drawing origins, held-gesture deferral, rapid strokes, late completion after panel navigation, focus loss, thumbnail suspension, export overlay isolation, frame formatting, grouped light-table fading, reference-image fading, active-layer changes, shared toggle/history controls and Font Awesome icons. Cached 300-stroke native workspace redraw measured approximately 2.8 ms/frame in this fixture; this is not a full interactive latency benchmark.
- PlaybackSmoke: 28 checks with native audio enabled, including silent transport, pause/resume/stop/loop/seek, Shot List controls, 200 px docks, audio attachment without scrubbing, sample consumption and cross-window tool/light-table synchronization. The synthetic tone produced a meter reading around -28 dB.
- MoviePlayerSmoke: real encoding, decoded video frames, duration, seek, absolute Unicode path copy and player actions. The native player image and Finder file selection were also inspected through the UI. QWidget::grab omits the native video surface; use an OS window capture or inspect the video sink when validating the displayed image.
- ExportMovieSmoke: the actual MainWindow Export Movie path rendered a five-frame silent project, encoded an MP4, opened the player, retained the 200 ms duration and restored the editing cursor.

The new ShotListPresentation and MoviePlayerWindow sources, PlaybackTiming header, and Qt Multimedia modules are included in the active Qt 6 project and root project definition. Existing user project files and unrelated untracked documents/build directories were preserved.
