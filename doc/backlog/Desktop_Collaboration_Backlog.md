# Desktop Collaboration Backlog

## UI Polish

- Replace the current text-based collaboration helper messages with compact visual status badges.
- Revisit avatar sizing, overlap and spacing once the collaboration panel layout stabilizes on desktop and laptop widths.
- Add optional richer identity chips for `Editor` and `Viewers` when real user profile data becomes available.
- Add a distinct workspace icon in the document tree so workspace roots do not look identical to regular document folders.

## Behavior

- Decide whether viewer presence should distinguish between "document opened" and "document focused".
- Decide whether collaboration warnings should auto-clear after successful lock reacquisition.
- Add document duplication from the desktop tree/context menu.
- Support moving documents across workspaces, not only within the current workspace tree branch.

## Sync Preparation

- Revisit collaboration panel states once Couchbase Sync replication status is available.
- Add a dedicated sync/replication indicator instead of overloading the collaboration hint line.
