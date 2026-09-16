# Boarder Web MVP Plan (Bootstrap-First)

## Objective
Deliver a fast, production-oriented web MVP using:
- **Web-native frontend** (no Qt/WASM UI)
- **Bootstrap-first UI shell** for speed
- **Flask backend** with collaboration + jobs
- **Cloud API and storage as source of truth**

## Why Bootstrap-First
Bootstrap is a good fit for your workflow and MVP speed.

Use Bootstrap for:
- App shell layout
- Nav, toolbars, forms, panels, modals
- Auth/project management pages
- Layer/shot properties panels

Do **not** rely on Bootstrap for:
- Drawing surface
- Timeline interaction layer
- High-performance editor overlays

These should be custom components (Canvas/WebGL).

## Recommended Stack

### Frontend (`boarder-web`)
- React + TypeScript + Vite
- Bootstrap + React-Bootstrap (or plain Bootstrap classes)
- Canvas engine: PixiJS (recommended)
- State: Zustand (MVP) + React Query for API data
- Realtime: Socket.IO client

### Backend (`boarder-api`)
- Flask + Flask-SocketIO
- SQLAlchemy + Alembic
- PostgreSQL
- Redis (cache + pub/sub)
- S3-compatible storage

### Jobs (`boarder-jobs`)
- Celery + Redis broker
- Worker tasks for:
  - thumbnail generation
  - export jobs
  - AI jobs

## UI Architecture (Bootstrap + Canvas)

### Shell Layout
- Top navbar (Bootstrap)
- Left project/shot tree panel (Bootstrap cards/list groups)
- Center editor area:
  - Canvas viewport (custom)
  - Timeline strip (custom + Bootstrap container)
- Right properties dock (Bootstrap forms)

### Styling Strategy
- Bootstrap default theme for MVP
- Add CSS variables for:
  - brand colors
  - editor accent colors
  - panel spacing/radius
- Keep a small design token layer to avoid hard Bootstrap lock-in later

## Data and API Contract First
Before implementation, define:
1. OpenAPI routes for MVP entities
2. JSON schema for panel/layer payloads
3. Socket event contracts (`op_applied`, `presence_update`, `job_update`)

## MVP Vertical Slice Definition
A vertical slice must include end-to-end functionality:
- DB model
- API endpoints
- frontend UI and state
- realtime event broadcast (if applicable)
- tests

## First Vertical Slice (Recommended)

### Slice: "Panel Layer Text Node"
Implement text-node creation and rendering through full stack.

#### Scope
1. User opens panel in web editor
2. User picks Text mode
3. User clicks canvas and creates text node (`Text`, font, size, color)
4. Text node persists in backend
5. Text node reloads correctly
6. Text node update events broadcast to collaborators

#### Backend tasks
- Add `text_contents` JSON field or normalized table linked to layer
- Add endpoints:
  - `POST /api/v1/panels/{panel_id}/layers/{layer_id}/text-nodes`
  - `PATCH /api/v1/.../text-nodes/{id}`
  - `DELETE /api/v1/.../text-nodes/{id}`
- Validate payloads
- Emit socket event on create/update/delete

#### Frontend tasks
- Add Bootstrap toolbar toggle for Text mode
- Add Bootstrap modal for text input/font/size/color
- Render text node on canvas
- Save via API and optimistic update state
- Listen to socket events and reconcile state

#### Tests
- API unit tests for CRUD + validation
- Frontend component tests for modal + create flow
- Minimal integration test for load/render text nodes

## Milestone Plan (Bootstrap-first)

### Milestone 1: Foundation
- Repo scaffolding
- Auth
- Project/scene/shot/panel CRUD
- Bootstrap shell + placeholder editor

### Milestone 2: Editor Core
- Layer list + panel properties (Bootstrap)
- Canvas stroke editing
- Text-node vertical slice (above)

### Milestone 3: Collaboration
- Socket presence + operation broadcast
- Revision checks

### Milestone 4: Jobs
- Thumbnail worker
- Export job queue + status UI

## Risks and Controls

Risk: Bootstrap classes leak into canvas/editor logic
- Control: isolate canvas in dedicated components with minimal Bootstrap coupling

Risk: API drift between frontend and backend
- Control: generate typed clients from OpenAPI in CI

Risk: slow MVP due to over-design
- Control: enforce vertical slices with acceptance criteria and timeboxes

## Done Criteria for MVP
- Users can authenticate and open projects
- Users can edit panels (strokes + text nodes)
- Data persists in cloud backend
- Realtime updates function for at least 2 users
- Export job can be submitted and downloaded
