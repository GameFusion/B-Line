# Agent Boot Prompt: 101 Local Workflow + Docs (Desktop)

Copy/paste this prompt into a new Codex agent thread.

---

You are the implementation agent for **Stargit/Boarder desktop 101 local workflow and onboarding docs**.

## Scope
Improve first-time local user flow only (no cloud sync in this task):
1. Create a new project.
2. App shows a default first scene + shot + panel automatically.
3. User can immediately start drawing without additional setup.
4. Document the workflow in clear getting-started docs.

## Do Not Implement Yet
- No cloud sync/database endpoint.
- No production-grade backend integration.
- No large refactors outside the 101 workflow.

## Architecture Warmup Inputs
Use this mind map and code pointers first:
- `/Users/andreascarlen/GameFusion/Applications/Boarder/docs/agent-prompts/stargit-101-architecture-mindmap.md`

Primary files likely involved:
- `/Users/andreascarlen/GameFusion/Applications/Boarder/main.cpp`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/MainWindow.cpp`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/MainWindow.h`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/NewProjectDialog.cpp`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/ProjectContext.h`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/ScriptBreakdown.cpp`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/PaintCanvas.cpp`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/ShotPanelWidget.cpp`
- `/Users/andreascarlen/GameFusion/Applications/TimeLineProject/TimeLineView.cpp`

Past AI artifacts/context:
- `/Users/andreascarlen/GameFusion/Applications/Boarder/docs/web-native-architecture-plan.md`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/docs/web-native-bootstrap-mvp-plan.md`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/docs/agent-vertical-slice-plan.md`
- `/Users/andreascarlen/GameFusion/Applications/Boarder/file_history.txt`

## Candidate Features Context
Align with:
- `/Users/andreascarlen/GameFusion/Applications/Boarder/docs/candidate-feature-list.md`

Note: Production Container is a candidate feature and should be documented/planned, but not implemented in this task unless trivial and non-invasive.

## Required Execution Plan
1. Discovery:
- Identify current `newProject` path and where default scene/shot/panel creation currently happens or fails.
- Trace startup focus/tool state after project creation.
- Identify missing defaults that block first draw.

2. Implementation:
- Ensure new project bootstrap includes default scene/shot/panel if absent.
- Ensure UI selects the created panel and focuses drawing context.
- Ensure default layer/tool setup is usable for immediate first stroke.
- Add safe guards so loading/saving remains backward-compatible.

3. Documentation:
- Add/update a getting-started markdown with:
  - new project steps
  - expected default objects (scene/shot/panel)
  - first draw steps
  - troubleshooting for common failure states

4. Validation:
- Manual test matrix on at least one local platform build:
  - create project from scratch
  - confirm defaults appear
  - confirm draw works immediately
  - save and reload project; defaults persist

## Deliverables
Return:
1. Summary of changes.
2. Exact files modified.
3. Commands run.
4. Manual test results.
5. Known gaps and recommended next tasks.

## Non-negotiable constraints
- Keep changes focused on 101 onboarding workflow.
- Avoid unrelated refactors.
- Preserve existing behavior for non-101 workflows as much as possible.
