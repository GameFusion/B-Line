# Active-stroke latency — 22 September 2026

Follow-up: [DRAWING-PREVIEW-PERFORMANCE.md](DRAWING-PREVIEW-PERFORMANCE.md) records
worker-rendered previews, retained light-table backgrounds and newer measurements.
The measurements and implementation description below are the earlier baseline.

The active drawing path now reuses completed fitting work and retains the
unchanged scene background during a brush gesture. Input samples still reach the
active canvas immediately; fitting runs on its worker thread. The other editor,
camera preview and thumbnails remain deferred until release.

Shared dependency: plugandpaint `master` commit `9222997`. B-Line compiles these
sources directly from the sibling checkout; update both repositories together.

## Measurements

Measured on an Apple M3 Max, macOS 26.6.2, Qt 6.10.2, using the existing **Debug
build** (`-g`, without an optimization flag). These are synthetic input dispatch
to **Qt paint completion** times, not physical stylus-to-display latency. They
include software rasterization but exclude OS input collection and display
scanout. They are fixture results, not a production-project frame-rate guarantee.

The repeatable comparison uses the offscreen raster backend at 2× density, a
640 × 480 document and an 800 × 600 workspace. A precise 4 ms timer submits a
pressure brush with 20 assessed steps per segment. The actual input interval is
recorded: expensive paints delay timer delivery. All 5,632 submitted points per
run were accepted and included in a completed paint; all 52 strokes committed.

| Canvas / background | Gesture | Input-to-paint p95 before | After |
| --- | --- | ---: | ---: |
| Integrated / empty | 12 × 32 points | 2.14 ms | 1.10 ms |
| Integrated / empty | 1,024 points | 9.58 ms | 9.64 ms |
| Integrated / 300-stroke light table | 12 × 32 points | 44.11 ms | 0.81 ms |
| Integrated / 300-stroke light table | 1,024 points | 47.92 ms | 9.25 ms |
| Workspace / empty | 12 × 32 points | 10.89 ms | 1.59 ms |
| Workspace / empty | 1,024 points | 18.69 ms | 12.98 ms |
| Workspace / 300-stroke light table | 12 × 32 points | 87.97 ms | 1.42 ms |
| Workspace / 300-stroke light table | 1,024 points | 92.81 ms | 12.99 ms |

The separate growing-worker benchmark feeds four new samples per fit, up to
1,024 points. Its p95 fell from **107.82 ms to 2.84 ms**; median fell from 55.20 ms
to 1.95 ms. This timer includes pressure mapping and the same result fingerprint
callback in both builds. A complete uncached 1,024-point fit still takes about
113 ms: the improvement comes from reusing work, not weakening the fitter.
The final geometry/pressure fingerprint is identical to the baseline.

For long strokes, release-to-model-commit changed from 98–155 ms to 2–22 ms in
these four raster fixtures. This is model completion, not the time at which all
secondary views finish refreshing.

A native Cocoa spot check of the dense workspace's 12 short strokes measured
**37.59 → 0.80 ms p95**, with all 384 samples accepted and painted in both reported
runs. Native activation/occlusion can suppress painting. Interrupted runs with
missing paints were rejected; an additional baseline native attempt stalled in
the fixture's pending-worker wait and supplied no timing record. The final
harness waits with a deadline for both model completion and worker exit, and
rejects incomplete/occluded runs instead of reporting them as fast.

Raw distributions, counts, environment and baseline commit IDs are retained in
[benchmarks/stroke-latency-2026-09-22.json](benchmarks/stroke-latency-2026-09-22.json).

## Changes

- `plugandpaint/app/Worker`: retain raw controls and the smoothed prefix for each
  append-only stroke. Refit the last potentially changing ten-point segment and
  new segments, then smooth only the affected junctions. The original full fit
  remains available as the equivalence reference. No input samples are dropped,
  and the smoothing, segmentation, pressure mapping and final quality are kept.
- Worker timing is single-shot and armed by incoming samples. An idle pen no
  longer wakes the worker every 16 ms. Redundant curve copies, reassessment and
  per-segment diagnostic logging were removed from the hot path.
- `PaintArea` retains a viewport-sized background at the current display density
  while drawing. Cache identity includes stroke, document composite, visible
  bounds, zoom, density and light-table mode. Layer or view changes refresh it.
  The integrated light-table canvas uses this image directly.
- `PaintCanvas` draws that background directly and records only the changing
  stroke/overlays while the brush is down. It avoids serializing the background
  pixels into a new `QPicture` for each sample. Direct cache construction preserves
  layer transforms, grouped opacity/blending, reference images and off-canvas
  content. It does not rasterize or resize the document itself.
- The smoke-test makefile now tracks included headers automatically. This avoids
  stale inline accessors after shared canvas classes change layout.

## Validation and reproduction

From `build-vs2019-qt6/build/Qt_6_10_2_for_macOS-Debug`:

```sh
make -j6 -f ../../../tests/PaintCanvasSmoke.mk all StrokeLatency PaintCanvasSmoke PlaybackSmoke ExportMovieSmoke
QT_QPA_PLATFORM=offscreen ./StrokeLatency --verify
QT_QPA_PLATFORM=offscreen ./StrokeLatency --fit
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 ./StrokeLatency
QT_QPA_PLATFORM=offscreen ./PaintCanvasSmoke
QT_QPA_PLATFORM=offscreen ./PlaybackSmoke
QT_QPA_PLATFORM=offscreen ./ExportMovieSmoke
./PaintCanvasSmoke
./PlaybackSmoke --audio
./StrokeLatency --ui --workspace --dense --rapid
```

`--ui` runs one case. Omit `--workspace` for the integrated view, omit `--dense`
for an empty background, and omit `--rapid` for the 1,024-point continuous stroke.
Run timing cases sequentially, without a competing build or test process.

Validation: 1,095 exact incremental/full-fit comparisons across curved, sharp,
stationary, diagonal and looping paths; different sample batches, segment
boundaries, assessment resolutions and cache resets. PaintCanvasSmoke passes
79 checks offscreen and natively, including pixel-exact cached/full-scene
agreement, reference fading, opacity, off-canvas ink, invalidation, coordinate
alignment, rapid strokes, tablet pressure, focus loss and late completion.
PlaybackSmoke passes its 39 offscreen and 44 native/audio checks, and
ExportMovieSmoke passes the actual render/encode/player/cursor path. One native
run failed the fixture's fixed 100 ms focus handoff check;
the test now waits for the actual viewport handoff with a one-second deadline
and emits state diagnostics on failure. The application's focus policy is unchanged.

## Remaining costs / next measurement

Long pressure strokes still repaint the entire fitted stroke. The empty
integrated case consequently has essentially unchanged p95; measuring dirty
regions or a retained fitted-ink prefix is the next useful renderer experiment.
The first background-cache fill and the full refresh after release remain
expensive. The dense short-stroke raster input-to-paint maxima are still about
47–53 ms, despite the much lower p95. Do not read the steady-state result as a
sub-millisecond first-ink guarantee.

Next validation should use real pressure/tilt hardware and a production scene,
including taps, long gestures, fast successive strokes and focus changes. A
release-build comparison and physical first-ink/display measurement remain
separate work; this report makes no claim about either.
