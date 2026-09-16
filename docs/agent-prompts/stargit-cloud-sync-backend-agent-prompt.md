# Agent Boot Prompt: Cloud Sync + Backend Endpoint

Copy/paste this prompt into a new Codex agent thread.

---

You are the implementation agent for **Stargit/Boarder desktop cloud sync + backend endpoint**.

## Objective
Design and implement the first practical cloud-sync path from desktop project data to a backend service.

Start with a minimal, reliable sync slice:
1. Desktop exports a canonical project payload.
2. Backend accepts and stores project data through a versioned endpoint.
3. Desktop can pull and rehydrate the same project snapshot.
4. Sync metadata (project id, revision, updatedAt) is tracked deterministically.

## Current Context
- Desktop app repo:
  - `/Users/andreascarlen/GameFusion/Applications/Boarder`
- Existing docs/artifacts:
  - `/Users/andreascarlen/GameFusion/Applications/Boarder/docs/candidate-feature-list.md`
  - `/Users/andreascarlen/GameFusion/Applications/Boarder/docs/agent-prompts/stargit-101-architecture-mindmap.md`
  - `/Users/andreascarlen/GameFusion/Applications/Boarder/docs/web-native-architecture-plan.md`
  - `/Users/andreascarlen/GameFusion/Applications/Boarder/docs/web-native-bootstrap-mvp-plan.md`

## Scope (This Task)
1. Define a canonical sync payload for:
- project
- scenes
- shots
- panels
- layers (include enough for storyboard continuity)

2. Add backend endpoint(s) for initial sync:
- `POST /api/v1/projects/sync` (upsert snapshot)
- `GET /api/v1/projects/{projectId}` (fetch latest snapshot)

3. Implement desktop sync client path:
- push current project snapshot
- pull and load snapshot
- track local vs remote revision

4. Add docs:
- endpoint contract
- sample request/response
- local test workflow

## Non-Goals (for this first slice)
- Multi-user realtime collaboration
- Full conflict-resolution UI
- Background queue workers
- Production auth/RBAC hardening
- Production container implementation

## Technical Constraints
1. Keep this as a minimal vertical slice with low regression risk.
2. Prefer explicit revision checks (`revision_conflict`) over silent overwrites.
3. Keep payload schema stable and versioned.
4. Add deterministic error mapping:
- `400 invalid_request`
- `404 not_found`
- `409 revision_conflict`
5. Keep changes focused; avoid unrelated refactors.

## Discovery First (Required)
Before coding:
1. Map current desktop project save/load pipeline in code:
- where project JSON and scene files are written/read
- where panel/layer/stroke data is serialized
2. Identify exact insertion points for push/pull sync commands in UI/actions.
3. Propose a concrete payload contract and migration-safe defaults.

## Suggested File Targets

Desktop (`Boarder`):
- `/Users/andreascarlen/GameFusion/Applications/Boarder/MainWindow.cpp`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/MainWindow.h`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/ProjectContext.h`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/ScriptBreakdown.cpp`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/ScriptBreakdown.h`

Backend (if using existing Flask service in workspace):
- create/extend routes, service, repository modules under backend app
- add tests for sync endpoints and revision checks

## Execution Plan
1. Discovery summary and schema proposal.
2. Implement backend sync endpoints + validation.
3. Implement desktop push/pull integration.
4. Add revision handling and error propagation in UI/logging.
5. Add tests and manual validation checklist.
6. Document run steps and API examples.

## Manual Acceptance Checklist
1. Create local desktop project with scene/shot/panel/layer data.
2. Push sync succeeds and returns `projectId` + `revision`.
3. Modify locally and push again with correct revision.
4. Simulate stale revision and verify `409 revision_conflict`.
5. Pull snapshot into clean local state and verify data integrity.

## Deliverables
Return:
1. Summary of implementation.
2. Files changed.
3. Commands/tests run and results.
4. Example payload and endpoint responses.
5. Known risks and next-step recommendations.

## Optional Follow-Up (Do Not Implement Now)
- Evaluate open-source storyboard backend options and map compatibility with this contract.
