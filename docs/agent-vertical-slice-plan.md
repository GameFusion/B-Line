# Boarder Agent Execution Plan (Vertical Slice)

## Goal
Ship one complete vertical slice end-to-end:
**Text Node CRUD + Render + Realtime Sync**

This is the recommended first slice for the web port.

## Team Model (Agent-Based)
Use 3 agents in parallel with one integration owner.

1. **Agent A: Backend API**
2. **Agent B: Frontend UI/Canvas**
3. **Agent C: Contracts + Tests + CI**
4. **Integrator (you or one agent): merge and resolve contract mismatches**

## Branching
- `feat/web-text-node-api`
- `feat/web-text-node-ui`
- `feat/web-text-node-contracts-tests`
- Integration branch: `feat/web-text-node-slice`

## Slice Scope

### Functional
1. Create text node in selected layer
2. Update text content/style/position
3. Delete text node
4. Persist and reload
5. Broadcast updates to connected clients

### Non-Functional
1. Input validation
2. Typed API contracts
3. Basic unit/integration tests
4. CI passes

## Contract (source of truth)

### Text node model
- `id: string`
- `layerId: string`
- `text: string`
- `fontName: string`
- `fontSize: number`
- `color: string` (hex)
- `x: number`
- `y: number`
- `createdAt: string`
- `updatedAt: string`

### REST endpoints
- `POST /api/v1/panels/{panelId}/layers/{layerId}/text-nodes`
- `PATCH /api/v1/panels/{panelId}/layers/{layerId}/text-nodes/{textNodeId}`
- `DELETE /api/v1/panels/{panelId}/layers/{layerId}/text-nodes/{textNodeId}`
- `GET /api/v1/panels/{panelId}` (must include text nodes)

### Socket events
- `text_node_created`
- `text_node_updated`
- `text_node_deleted`

Payload includes panelId, layerId, textNode.

## Work Breakdown by Agent

### Agent A: Backend API (Flask)
1. Add DB model/migration for text node (or JSON strategy if already layer JSON)
2. Implement REST endpoints
3. Add schema validation
4. Emit SocketIO events on mutations
5. Add backend tests for CRUD + validation

**Acceptance**
- API returns valid JSON
- Errors are deterministic (`400/404/409`)
- Socket events emitted on all mutations

### Agent B: Frontend (React + Bootstrap + Canvas)
1. Add Text mode button in toolbar
2. Add Bootstrap modal/form for text/font/size/color
3. On canvas click in text mode, create text node via API
4. Render text nodes in layer renderer
5. Support drag/move text node and PATCH update
6. Handle delete action
7. Subscribe to socket events and update local state

**Acceptance**
- Create/update/delete works without page reload
- Realtime updates visible in second browser session

### Agent C: Contracts + Testing + CI
1. Add/update OpenAPI definitions for endpoints
2. Add schema for text node payloads
3. Generate frontend typed API client (if setup exists)
4. Add minimal integration test (API + UI render)
5. Ensure lint/type/test CI pipeline is green

**Acceptance**
- Contract and implementation match
- CI blocks contract drift

## Integration Sequence
1. Merge Agent C contract branch first
2. Rebase Agent A and Agent B on updated contracts
3. Merge Agent A (backend)
4. Merge Agent B (frontend)
5. Run integration tests and fix deltas
6. Cut slice release tag

## Risks / Guardrails
1. **Contract drift**
   - Guardrail: merge contracts first, generated types required
2. **Canvas and Bootstrap coupling**
   - Guardrail: Bootstrap only for shell/forms/modals, not render primitives
3. **Realtime race conditions**
   - Guardrail: include `revision` or `updatedAt` and last-write strategy for MVP

## Copy/Paste Prompts for Agents

### Prompt for Agent A (Backend)
Implement Flask endpoints and persistence for text nodes under panel/layer scope.
Add validation and SocketIO broadcasts for create/update/delete.
Follow existing API style in the repo. Add unit tests for success and validation failure paths.
Do not change unrelated endpoints.

### Prompt for Agent B (Frontend)
Implement Text mode in web editor using Bootstrap for the input modal and toolbar controls.
On canvas click in Text mode, create a text node through API and render it.
Support update (text/style/position) and delete.
Subscribe to socket events and reconcile local state.
Keep canvas logic decoupled from Bootstrap classes.

### Prompt for Agent C (Contracts/CI)
Define OpenAPI + JSON schema for text node CRUD endpoints and socket payloads.
Add or update generated client types used by frontend.
Add tests ensuring contract compatibility and CI checks for drift.
Do not implement UI or backend logic beyond contract/test harness.

## Definition of Done (Slice)
1. Text node CRUD works end-to-end
2. Text nodes persist and reload
3. Two clients see realtime updates
4. Tests and CI pass
5. Documentation updated (API + usage notes)
