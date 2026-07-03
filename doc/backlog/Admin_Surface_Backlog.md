# Admin Surface Backlog

## Decision Fixed For Later Implementation

- Workspace creation is currently exposed in the desktop client only as a temporary test-only control.
- The main `cppwiki` desktop application must not become the long-term surface for administrative operations.
- A separate `cppwiki-admin` application will be introduced later for admin workflows.

## Scope Boundary

### Main desktop app `cppwiki`

- document browsing and editing;
- sync bootstrap consumption;
- presence and lock flows;
- no permanent admin controls.

### Future admin app `cppwiki-admin`

- workspace creation;
- future workspace management operations;
- future role/channel/policy administration;
- future operational and diagnostic admin actions.

## Migration Rule

- The current `New workspace` button in the desktop client stays only for short-term testing.
- When `cppwiki-admin` exists, workspace creation must move there and be removed from the main desktop UI.

## Non-Goals For Now

- Do not scaffold `cppwiki-admin` yet.
- Do not expand the temporary desktop admin control beyond workspace creation testing.
