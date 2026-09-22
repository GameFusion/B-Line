# Windows smoke tests

Build the release application and its shared GameEngine libraries first, using
the same MSVC toolset throughout. From an x64 Visual Studio developer prompt in
`build-vs2019-qt6`, with Qt's `bin` and `jom` on `PATH`:

```bat
qmake ..\tests\Smoke.pro -o Makefile.PlaybackSmoke SMOKE_TEST=PlaybackSmoke CONFIG+=release CONFIG-=debug CONFIG-=debug_and_release
jom -f Makefile.PlaybackSmoke -j 8
set QT_QPA_PLATFORM=
PlaybackSmoke.exe
```

The same project supports `PaintCanvasSmoke`, `StrokePreviewSmoke`,
`TimelinePaintSmoke`, `MoviePlayerSmoke`, `ExportMovieSmoke`, and
`ShotListAccessibilitySmoke`, and `UndoHistorySmoke`. Substitute the
name in all three places above. Test objects reuse the application build output.
The normal application Makefile remains available for `jom -f Makefile`.

Runtime DLLs must be on `PATH` (Qt and the application's FFmpeg 4.x shared
libraries), or deployed beside the executable with `windeployqt`. Movie/export
tests also require the `ffmpeg.exe` encoder on `PATH` or beside the test binary.
When using DLLs from a separate deployed runtime directory, set `QT_PLUGIN_PATH`
to that directory too. Setting only `QT_QPA_PLATFORM_PLUGIN_PATH` loads the window
plugin but does not make the multimedia plugins available to the movie tests.

Run `PlaybackSmoke.exe --audio` to include native audio verification. This plays
a short synthetic tone. The fixtures use temporary projects and isolated settings
rather than changing a real project.

Use native windows for the drawing and timeline tests on Windows. The offscreen
plugin lacks system font discovery and has different paint-region behavior;
launching the timeline test with a hidden window also suppresses the paint events
it measures. Playback, stroke-preview, movie-player and export fixtures can also
run with `QT_QPA_PLATFORM=offscreen`.

`ShotListAccessibilitySmoke` runs with the native Windows platform. It primes
Qt's accessibility cache, collapses SCENE_006, selects a shot in SCENE_008, and
checks bounded sibling traversal before another event-loop pass. It also covers
repeated collapse/expand and collapse/expand all. The original Qt 6.10.2 behavior
failed with `accessible child 26 maps back to 0`, which can trap Windows UI
Automation in a sibling cycle. Set `QT_QPA_PLATFORM_PLUGIN_PATH` to the Qt
installation's `plugins/platforms` directory if the build folder lacks plugins.

`UndoHistorySmoke` uses Qt Test and the native Windows platform to dispatch
keyboard and mouse input inside an isolated fixture. It checks that there is
one populated Edit menu, draws real strokes, and exercises Ctrl+Z,
Ctrl+Shift+Z and Windows Ctrl+Y in the integrated paint area and separate drawing
workspace. It also verifies named paint/layer actions in Edit > Undo History,
clicking earlier/later history entries, and replacing the redo branch after a
new edit. Qt's `bin` directory must be on `PATH` for `Qt6Test.dll`.
