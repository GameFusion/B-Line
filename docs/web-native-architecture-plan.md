# Boarder Web-Native Architecture Plan

## Goal
Build a **web-native** version of Boarder from the start, with cloud-hosted data and server-side heavy processing (AI, rendering, exports), while maximizing reuse of stable C++ logic on the backend.

## Architecture Overview

### Repositories
1. `boarder-web`
   - React + TypeScript + Vite
   - Canvas editor UI (PixiJS or Konva)
   - Timeline/layers/shot panels
   - WebSocket collaboration client

2. `boarder-api`
   - Python Flask API (`/api/v1/...`)
   - Flask-SocketIO for real-time collaboration/presence
   - Auth, project CRUD, permissions, revisioning

3. `boarder-jobs`
   - Python Celery workers
   - Redis broker + task queue
   - AI jobs, rendering, exports, thumbnail generation

4. `boarder-core-cpp`
   - Shared C++ core algorithms (server-side only)
   - Exposed via CLI and/or `pybind11`
   - Deterministic math/geometry/media transforms

5. `boarder-shared-contracts`
   - OpenAPI specs
   - JSON Schemas for payloads/events
   - Generated TS/Python SDKs

6. `boarder-infra`
   - Docker / Compose / Kubernetes
   - IaC and CI/CD templates
   - Observability and environment configs

## Technology Choices

### Frontend
- React + TypeScript
- PixiJS (recommended) or Konva for canvas drawing
- State: Zustand (fast MVP) or Redux Toolkit
- IndexedDB cache for local draft buffering

### Backend
- Flask + Flask-SocketIO
- PostgreSQL (metadata + revision history)
- Redis (cache, pub/sub, Celery broker)
- S3-compatible object storage for assets/artifacts

### Workers
- Celery + Redis
- Optional GPU worker pool for AI/render jobs

### Shared Logic
- Keep non-UI core logic in C++
- Use Python integration via CLI or bindings
- No WebAssembly dependency for the browser UI

## Data Model Direction

### Layer Content
Layer should support both:
- `strokes` (existing)
- `textContents` (new)

`textContents` item fields:
- `text` (string)
- `fontName` (string)
- `fontSize` (number)
- `color` (string, hex)
- `x` (number)
- `y` (number)

All schema fields should be API-contract driven from `boarder-shared-contracts`.

## Collaboration Model
1. Server-authoritative operation stream
2. Client submits atomic operations (insert stroke, move keyframe, add text node, etc.)
3. Backend validates, stores revision, broadcasts via WebSocket
4. MVP conflict strategy: optimistic concurrency with revision checks
5. Optional future: CRDT for deeper concurrent editing

## Server-Side Heavy Work
All heavy/long-running tasks should be async jobs:
- AI generation
- Render/export (video/pdf/image sequences)
- Thumbnail generation
- Batch transforms

Pattern:
1. API enqueues job
2. API returns `jobId`
3. Worker processes job
4. Client polls or subscribes for completion events
5. Result stored in object storage + metadata in DB

## Migration Plan

### Phase 1: Foundation (3-5 weeks)
- Bootstrap repos and contracts
- Implement auth + project/scene/shot/panel/layer CRUD
- Configure Postgres + Redis + S3 + Celery

### Phase 2: Web Editor MVP (5-8 weeks)
- Canvas editor (strokes/layers/camera)
- Timeline basics
- Save/load via API
- Live updates over SocketIO

### Phase 3: Heavy Jobs + Collaboration Hardening (4-6 weeks)
- Render/export/AI workers
- Job status/eventing
- Revision history + undo/redo based on ops

### Phase 4: Production Readiness (3-5 weeks)
- RBAC/permissions
- Audit logging
- Monitoring/alerts
- Performance for large projects

### Phase 5: Desktop Convergence (optional)
- Desktop clients consume same cloud API
- Gradual retirement of local-only paths

## Bootstrap for MVP: Recommendation
Yes, Bootstrap is a good choice for fast MVP delivery.

Recommended approach:
1. Use Bootstrap for all shell/admin UI (menus, forms, dialogs, layout, tables).
2. Keep drawing/timeline/editor surfaces as custom canvas components (not Bootstrap widgets).
3. Add a light design-token layer (CSS vars) so you can re-theme later without rewrites.
4. Avoid coupling editor logic to Bootstrap classes.

This gives speed now and flexibility later.

## Codex-Agent Implementation Strategy
Using Codex agent is a strong approach if you structure work into clear vertical slices.

Recommended workflow:
1. Define milestone tickets with explicit acceptance criteria.
2. Keep API contracts first (OpenAPI + JSON schema) before implementation.
3. Implement in thin slices:
   - API endpoint
   - DB model
   - frontend call
   - UI state + tests
4. Enforce CI checks per PR:
   - lint
   - type checks
   - unit tests
   - contract tests
5. Prefer small PRs (1 feature at a time) to reduce regressions.

## Initial MVP Feature Slice Order
1. Auth + project list
2. Scene/shot/panel CRUD
3. Layer CRUD + ordering + visibility
4. Canvas stroke create/edit/delete
5. Text nodes (create/edit/delete)
6. Timeline scrub/playback data integration
7. Thumbnails and preview pipeline
8. Export job submission + result retrieval

## Notes
- Primary product direction: web-native UI + cloud APIs
- No dependency on Qt WebAssembly for final web product
- Shared C++ logic remains valuable on backend services/workers
