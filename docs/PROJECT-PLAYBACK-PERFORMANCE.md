# Production-project playback — 22 September 2026

This pass measures the project supplied by the user: 7 scenes, 60 shots, 63 panels,
134 layers (8 animated), camera moves, reference images and multiple audio tracks.
The visual sequence is 37.165 seconds at 24 fps, with a 1536 × 864 canvas and
960 × 540 output. Tests load a disposable full copy and use temporary QSettings;
the production artwork and script are not included in this repository.

## Results

Apple M3 Max, macOS 26.6.2, Qt 6.10.2, existing **Debug build** (`-g`, no compiler
optimization flag). The repeatable comparison uses Qt's offscreen raster backend
at 2× density, a 1440 × 900 main window and a 1200 × 800 workspace fitted to the
canvas. Each run plays the complete sequence in real time, with the actual
MainWindow transport, shot changes, timeline, camera preview and FPS display.
Audio is disabled for the paired raster measurements. Runs are sequential,
without a competing build/test or sampling profiler.

| Metric | Before | After |
| --- | ---: | ---: |
| Standalone workspace, distinct frames / second | 21.68 | 23.94 |
| Integrated canvas, distinct frames / second | 23.01 | 24.00 |
| Workspace paint, median | 11.48 ms | 3.80 ms |
| Workspace paint, p95 | 32.12 ms | 7.16 ms |
| Timeline paint during workspace playback, p95 | 41.05 ms | 8.81 ms |
| Integrated canvas paint events over the sequence | 32,889 | 893 |
| Integrated canvas total paint time | 12.00 s | 3.62 s |

Every run displays all 63 panels. The final workspace run paints 890 distinct
frames; the integrated run paints all 892 sequence frames. No frames are credited
to the wrong playback viewport. Frame count ignores duplicate paint events for
the same timeline frame, including overlay/expose repaints.

These are elapsed-clock **Qt paint completion** measurements, not physical display
scanout or an A/V synchronization guarantee. The earlier profiled workspace
baseline (21.39 fps) was repeated without sampling; the unprofiled 21.68 fps run is
used above. Raw distributions, counts, environment and baseline dependency commits
are in [benchmarks/project-playback-2026-09-22.json](benchmarks/project-playback-2026-09-22.json).

Native Cocoa runs with audio enabled also display all 63 panels: **23.96 fps**
in the workspace (891 distinct frames), **23.99 fps** in the main view (892
frames). Workspace paint p95 is 4.58 ms; main paint p95 is 2.23 ms. The audio
stream position advances in both runs. The workspace run reaches natural End
before reporting, so its final `audio_playing` value is false; the main run is
still playing at the reporting deadline. The native baseline without audio is
retained in the raw data as additional context, not as an identical-configuration
comparison with these audio-enabled runs.

## Changes

- `PaintArea::viewportBackground` extends the existing stroke background cache to
  playback. It retains only the visible region at screen density and draws pixels
  outside the per-frame `QPicture`. A static panel no longer repeatedly encodes
  and decodes reference/background images while timecode and camera controls
  change. Layers still use their authored transforms, opacity and blend modes;
  the document remains editable and off-canvas drawing is preserved.
- Cache identity covers composite revision, active light-table layer, bounds,
  zoom, density and stroke state. Motion-blur effects also track evaluation time,
  including the shutter tail after the final transform keyframe. Animated layers,
  panel changes, pan/zoom and light-table changes invalidate the cached pixels.
- The integrated camera overlay updates only for changed image, text or geometry.
  Its fully covered preview is opaque to Qt, so updating its timecode does not
  repaint the underlying canvas. This removes a continuous transparent-overlay
  repaint loop that occurred even while paused.
- The timeline header formats/draws only exposed ticks, retaining enough left
  overhang for partially visible labels. It uses Qt's
  [extended style option and exposed rectangle](https://doc.qt.io/qt-6/qstyleoptiongraphicsitem.html#exposedRect-var).
  Moving the cursor lets the scene invalidate its old/new bounds instead of
  repainting every waveform. Label geometry changes notify the scene before
  changing bounds, so growing/shrinking text is erased correctly.

The focus/visibility playback policy, audio routing, elapsed-time transport,
frame rate, camera animation and exported movie rendering are unchanged.
B-Line, plugandpaint and TimeLineProject must be updated together. Shared
dependency commits: plugandpaint `41f9a01` and TimeLineProject `ad3d477`.

## Verification and reproduction

Build the normal app and test executables from the existing shadow-build directory:

```sh
make -j6 -f ../../../tests/PaintCanvasSmoke.mk all ProjectPlaybackBenchmark TimelinePaintSmoke PaintCanvasSmoke PlaybackSmoke ExportMovieSmoke StrokeLatency
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 ./ProjectPlaybackBenchmark --project /path/to/disposable/project --disposable-copy --workspace
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 ./ProjectPlaybackBenchmark --project /path/to/disposable/project --disposable-copy
./ProjectPlaybackBenchmark --project /path/to/disposable/project --disposable-copy --workspace --audio
```

Use a **complete disposable copy**, including media; the regular loader can
write thumbnails/cache files. Rewrite any absolute audio paths inside the copy
to point to its copied media. The benchmark requires `--disposable-copy` as an
explicit acknowledgment. `--duration-ms` and `--start-ms` select a shorter range.
Settings use temporary INI files instead of the user's recent-project/window
preferences. Native runs need an exposed, focused target window; the tool rejects
wrong-view results and empty paint sequences. Extract JSON lines from the app's
mixed diagnostic output with `rg '^\{'`.

Validation:

- PaintCanvasSmoke: **86 checks**, offscreen and native, including exact cached /
  uncached image comparisons, reference/background fading, off-canvas ink,
  opacity, light-table selection, animated layers, panel replacement and export
  isolation, plus existing mouse/tablet/worker-completion checks.
- TimelinePaintSmoke: **80 exact header image comparisons** across four time
  formats, four zoom levels and five scroll positions, plus a cursor dirty-region
  check. Native cursor update spans 50 of 800 viewport pixels; offscreen spans
  57 of 780. Both backends pass.
- PlaybackSmoke: **40 offscreen / 45 native-audio checks**, including focus
  handoff, hidden/minimized views, pause/resume/seek/loop/end, measured FPS,
  frozen secondary previews, idle repaint-loop regression and native audio
  consumption.
- ExportMovieSmoke: actual render, encode, movie player, silent-project handling,
  cursor restoration and duration pass.
- StrokeLatency: **1,095 exact incremental/full-fit comparisons** pass after the
  shared canvas cache changes.

## Remaining costs

This pass restores approximately the 24 fps target but does not eliminate every
hitch. The workspace's worst paint remains about 122 ms; its longest frame gap is
190 ms. The main viewport's longest frame gap is about 85 ms. First-time panel
background construction, image/layer loading and timeline auto-scroll remain
visible in those tails. A bounded next-panel prefetch/cache is a possible next
experiment; it needs its own memory/invalidation measurements.

The shared changes were measured in the existing Debug configuration, not a
Release build. Long-duration A/V drift, physical pen-to-display latency and
multi-track audio mixing are separate work. Current playback still uses its
existing first audio track.
