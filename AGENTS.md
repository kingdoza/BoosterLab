# AGENTS.md

## Role

This file is the repository-level entry point for AI agents working on FirstPersonTemplate.

Do not treat this file as the architecture source of truth. It only points agents to the current canonical project documents.

## Canonical Documents

Read the relevant `.md/` documents before making architecture, implementation, or review decisions.

- `.md/AGENT_ARCHITECTURE.md`: architecture/documentation agent rules
- `.md/AGENT_IMPLEMENTATION.md`: implementation agent rules
- `.md/AGENT_REVIEW.md`: review agent rules
- `.md/0_ARCHITECTURE.md`: current architecture map
- `.md/Architecture/*.md`: system-specific architecture documents

## Scope

FirstPersonTemplate is an Unreal Engine project. The primary source scope is:

- `Source/FirstPersonTemplate/Public`
- `Source/FirstPersonTemplate/Private`

`Content/` contains Blueprint/assets and must be treated as serialized project data. Do not modify or resave assets unless the active task explicitly requires it.

Generated or local-runtime directories are not architecture sources of truth:

- `Intermediate/`
- `DerivedDataCache/`
- `Saved/`
- `Binaries/`

## Working Rules

- Follow the task-specific `.md/AGENT_*.md` file for the current role.
- Use `.md/0_ARCHITECTURE.md` as the system map and open the relevant `.md/Architecture/*System.md` files for details.
- Keep `AGENTS.md` short. Do not duplicate system inventories, class lists, or workflow details here.
- If this file conflicts with `.md/` canonical documents, prefer the `.md/` documents and update this file only as a routing entry point.
