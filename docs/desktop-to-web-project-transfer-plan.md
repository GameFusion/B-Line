# Boarder Desktop To Web Project Transfer Plan

## Priority

This is the highest-priority migration track.

The goal is not a slow manual recreation workflow. The goal is:

1. Take an existing Boarder desktop project.
2. Upload it to the web platform with minimal friction.
3. Open the same project structure in web with the same creative intent preserved.

## Product Decision

The transfer path must be **project-first**, not production-first.

For now:

- the transferable unit is a **project**
- production container remains a later feature
- project upload must work without introducing a production wrapper

This keeps the path fast and reduces schema churn.

## Target Outcome

A storyboard artist should be able to:

1. Open a desktop Boarder project.
2. Choose `Upload To Cloud` or `Export For Web`.
3. Package project metadata, timeline structure, panels, layers, strokes, text, cameras, thumbnails, audio references, and assets.
4. Send that package to the web backend.
5. See the same project open in the web application with the same scene/shot/panel structure and usable media.

## Migration Principle

Do not make the web database the first source of truth for conversion rules.

Instead:

1. Define a **canonical transfer package** for a Boarder project.
2. Make desktop export to that package.
3. Make web import from that package.
4. Only then persist imported data to the web backend storage model.

That gives one stable seam between desktop and web, and it avoids locking the migration logic to an early database schema.

## Recommended Architecture

### 1. Canonical Transfer Package

Create a versioned package format such as:

- `boarder-project-package.json`
- `assets/`
- `thumbnails/`
- `audio/`

The package should contain:

- project metadata
- resolution/canvas settings
- scenes
- shots
- panels
- layers
- vector strokes
- text nodes
- camera frames and animation
- timeline timing
- audio tracks and segments
- asset manifest
- schema version

This package can be represented as:

- a zip archive for upload
- or a directory export during development

### 2. Desktop Exporter

Desktop Boarder should gain a dedicated exporter layer that reads the current project model and serializes the canonical package.

This exporter should be isolated from UI code as much as possible.

Suggested output responsibilities:

- normalize UUIDs and parent relationships
- gather local files and asset references
- render or gather thumbnails
- copy or reference audio/image assets
- write a manifest with checksums and schema version

### 3. Web Import Pipeline

The web backend should expose an import endpoint such as:

- `POST /api/v1/projects/import`

The import flow should:

1. receive package
2. validate schema version
3. unpack assets
4. create project shell
5. import hierarchy in deterministic order
6. attach media/assets
7. generate any missing derived previews
8. return new web project id plus import report

### 4. Compatibility Layer

The first import implementation should aim for **desktop parity import**, not a generic open interchange format.

That means:

- optimize for Boarder desktop -> Boarder web
- not for third-party interchange
- not for arbitrary external storyboard formats

## Entity Mapping

The migration seam should preserve these entities directly.

| Desktop concept | Canonical package | Web target |
| --- | --- | --- |
| Project | `project` | web project |
| Scene | `scenes[]` | scene rows/documents |
| Shot | `shots[]` or nested under scene | shot rows/documents |
| Panel | `panels[]` or nested under shot | panel rows/documents |
| Layer | `layers[]` | layer rows/documents |
| Stroke | vector stroke payload | stroke rows/documents |
| Text node | text payload | text node rows/documents |
| Camera frame | camera payload | camera/camera keyframes |
| Audio track | audio track payload | audio track rows/documents |
| Audio segment | audio segment payload | audio clip/segment rows/documents |
| Asset file | manifest + file blob | object storage/media record |

## Phase Plan

### Phase 0: Inventory And Freeze Desktop Semantics

Output:

- exact list of desktop entities to preserve
- current save/load paths and files
- risky fields and desktop-only behavior
- first draft canonical schema

This phase is documentation and extraction planning only.

### Phase 1: Canonical Export Package

Output:

- desktop exporter that writes a package to disk
- schema version `v1`
- import sample fixtures for testing

Success criteria:

- export a real desktop project
- inspect package manually
- package can be re-read deterministically

### Phase 2: Web Import Endpoint

Output:

- upload endpoint
- package validator
- importer service
- import report

Success criteria:

- upload a real exported project
- create a web project
- hierarchy matches desktop

### Phase 3: Visual Parity Validation

Output:

- panel preview comparison workflow
- stroke/text/camera validation checklist
- audio track import validation

Success criteria:

- first frame and panel previews are acceptably close
- timing and hierarchy are preserved
- no missing assets in common projects

### Phase 4: Desktop Upload UX

Output:

- desktop menu action for export/upload
- progress UI
- error reporting
- retry/failure log

Success criteria:

- artist can upload without using dev tooling

### Phase 5: Incremental Sync

Only after one-shot import is stable.

Output:

- changed-since-last-upload detection
- delta upload
- conflict strategy

This is explicitly **after** full project transfer works.

## What To Preserve First

The first transfer milestone should preserve:

1. project metadata
2. scene/shot/panel hierarchy
3. panel timing
4. layers
5. vector strokes
6. text nodes
7. camera frames / camera animation
8. thumbnails/previews

Audio can be included in package design now, but it does not need to block the first import if doing so slows the first end-to-end transfer.

## What Not To Block On

Do not block project transfer on:

- production container
- realtime collaboration
- partial sync
- permissions model
- final cloud publishing workflow
- generalized interchange with other storyboard tools

## Risks

### 1. Desktop Save Model Drift

If export logic is built directly against scattered UI save code, migration becomes fragile.

Mitigation:

- centralize export mapping
- keep one canonical package writer

### 2. Asset Path Fragility

Desktop projects may reference local absolute or relative paths inconsistently.

Mitigation:

- package assets into the export
- avoid path-only imports for critical media

### 3. Schema Lock-In Too Early

If web DB schema becomes the transfer contract too early, iteration slows down.

Mitigation:

- keep transfer package versioned and independent

### 4. Thumbnail/Preview Mismatch

Visual trust matters. Artists will judge import quality by what they see immediately.

Mitigation:

- validate imported previews against desktop output
- preserve camera framing and panel composition first

## Immediate Execution Plan

### Track A: Desktop Discovery

1. inventory current project save/load code
2. identify all persisted project files
3. map desktop runtime entities to saved representation
4. draft canonical transfer schema `v1`

### Track B: Canonical Package Spec

1. write schema markdown
2. define folder/archive layout
3. define asset manifest format
4. define importer validation rules

### Track C: Export Prototype

1. export one desktop project to package
2. inspect output
3. correct missing entities
4. lock first sample fixture

### Track D: Web Import Prototype

1. create import endpoint
2. unpack and validate package
3. create project in web backend
4. render project in web UI

## Ultra-Priority Recommendation

If speed is the priority, the fastest strategic move is:

1. build a **desktop exporter**
2. build a **web importer**
3. move real projects across immediately

Do not wait for a perfect unified system before testing transfer with real user projects.

Real project import will expose the right missing fields faster than further abstract planning.

## Next Deliverables

The next concrete documents/code tasks should be:

1. `desktop-project-save-load-inventory.md`
2. `canonical-project-package-v1.md`
3. `desktop-exporter-task-breakdown.md`
4. `web-importer-task-breakdown.md`
5. one exported sample package from a real Boarder project
