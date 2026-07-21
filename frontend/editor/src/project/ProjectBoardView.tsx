// Project board document kind (ADR-017 follow-up, issue #106): a single shared task list
// rendered through three switchable views (Gantt/Kanban/DataGrid), all backed by SVAR's
// open-source (MIT) React component suite (svar.dev). All three read/write the SAME
// `tasks`/`columns` arrays (see ./projectBoard.ts) — editing a task's dates in Gantt, or
// dragging its card to a different Kanban column, is the same underlying data as a row in the
// DataGrid view. Only one tab is mounted at a time (conditional rendering below), so switching
// tabs always remounts the next view with the latest shared state rather than needing live
// cross-component sync while multiple are mounted simultaneously.
import { useEffect, useMemo, useRef, useState } from "react";

import { Editor as GanttEditor, Gantt, WillowDark as GanttTheme, getEditorItems as getGanttEditorItems } from "@svar-ui/react-gantt";
import "@svar-ui/react-gantt/all.css";
import {
  Editor as KanbanEditor,
  Kanban,
  WillowDark as KanbanTheme,
  getEditorItems as getKanbanEditorItems,
  getPriorityOptions,
} from "@svar-ui/react-kanban";
import "@svar-ui/react-kanban/all.css";
import { Grid, WillowDark as GridTheme } from "@svar-ui/react-grid";
import "@svar-ui/react-grid/all.css";

import type { EditorBridge } from "../bridge/editorBridge";
import { snapshotDebounceMs } from "../constants";
import {
  fromParsedTasks,
  makeColumnId,
  makeTaskId,
  parseProjectBoardJson,
  toParsedTasks,
  type ParsedProjectTask,
  type ProjectBoard,
  type ProjectColumn,
  type ProjectTask,
} from "./projectBoard";

type ViewMode = "gantt" | "kanban" | "grid";

// SVAR's IApi/KanbanInstanceApi types are generic over each package's internal store action
// config; typing them precisely here isn't worth the friction, hence the `any`s below.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
type SvarApi = any;

// Kanban's built-in Low/Medium/High priority levels are the only per-card "color" knob the
// library exposes (each level maps to a colored card accent) — this is what "changing a card's
// color" maps onto in our schema (ProjectTask.priority). Every key must be listed explicitly:
// passing any shape object at all replaces Kanban's defaults wholesale rather than merging with
// them, so a key missing here is the same as turning it off (this is what silently dropped
// `progress` before it was added back).
//
// tags/users stay off here: SVAR's built-in multicombo field only lets you *pick* from a fixed
// `options` list passed in up front — there's no "type a new one" support, so an empty options
// list (this app has no predefined tag vocabulary or user directory) makes it impossible to add
// anything at all. Plain free-text fields are added separately below instead (tagsText/usersText,
// see kanbanEditorItems), which our own read/write mapping in KanbanTab turns into `tags`/`users`
// string arrays.
const kanbanCardShape = {
  priority: { data: getPriorityOptions() },
  progress: true,
  description: true,
  deadline: true,
  tags: false,
  users: false,
};

const kanbanEditorItems = [
  ...getKanbanEditorItems(kanbanCardShape),
  { comp: "text", key: "tagsText", label: "Tags (comma-separated)" },
  { comp: "text", key: "usersText", label: "Assignees (comma-separated)" },
];

function splitCommaList(value: string | undefined): string[] | undefined {
  if (!value) {
    return undefined;
  }
  const items = value
    .split(",")
    .map((item) => item.trim())
    .filter(Boolean);
  return items.length ? items : undefined;
}

function GanttTab({
  tasks,
  onChange,
}: {
  tasks: ParsedProjectTask[];
  onChange: (tasks: ParsedProjectTask[]) => void;
}) {
  // `useState` (not `useRef`) so mounting <GanttEditor> below re-renders once the api is ready —
  // it's null on the very first render, before Gantt's `init` callback fires post-mount.
  const [api, setApi] = useState<SvarApi>(null);

  useEffect(() => {
    if (!api) {
      return;
    }
    const pushChange = () => {
      const serialized = api.serialize({ data: "tasks" }) as ParsedProjectTask[] | null;
      if (serialized) {
        onChange(serialized);
      }
    };
    api.on("add-task", pushChange);
    api.on("update-task", pushChange);
    api.on("delete-task", pushChange);
    api.on("move-task", pushChange);
    // drag-task fires continuously while dragging (ev.inProgress === true mid-drag); only
    // persist once the drag/resize has actually settled.
    api.on("drag-task", (ev: { inProgress?: boolean }) => {
      if (!ev.inProgress) {
        pushChange();
      }
    });
    // Editor writes task field changes (rename, dates, progress, ...) via "update-task" too, so
    // the same listener above already covers it — no separate hookup needed.
    // eslint-disable-next-line react-hooks/exhaustive-deps -- `api` is stable once set;
    // re-running this on every `tasks`/`onChange` change would attach duplicate listeners.
  }, [api]);

  return (
    // `fonts={false}`: WillowDark's default (`fonts: true`) injects a <link> to
    // https://cdn.svar.dev's icon font — this app is offline-first and never makes ad hoc
    // external network calls, and that fetch silently failing was also leaving icon-dependent
    // controls (chevrons, calendar glyphs) rendering as blank boxes.
    <GanttTheme fonts={false}>
      {/* `placement="inline"` renders the editor in normal flow as a sibling of the board (see
          .project-board-tab-layout in styles.css), sized to match the widget's own height,
          instead of the default `placement="sidebar"`, which is a viewport-fixed drawer that
          doesn't compose with this app's own layout (wrong height, floating close button). Gantt's
          own Editor wrapper renders nothing at all until a task is open for editing, so this
          costs no extra space otherwise. */}
      <div className="project-board-tab-layout">
        <div className="project-board-tab-widget">
          <Gantt init={setApi} tasks={tasks} />
        </div>
        {/* Double-clicking a task opens this automatically (SVAR's built-in behavior) — no
            separate wiring needed beyond mounting it alongside Gantt with the same api. */}
        {api ? <GanttEditor api={api} items={getGanttEditorItems()} placement="inline" /> : null}
      </div>
    </GanttTheme>
  );
}

function KanbanTab({
  tasks,
  columns,
  onChange,
}: {
  tasks: ParsedProjectTask[];
  columns: ProjectColumn[];
  onChange: (tasks: ParsedProjectTask[]) => void;
}) {
  // See GanttTab's identical comment on why this is useState, not useRef.
  const [api, setApi] = useState<SvarApi>(null);

  useEffect(() => {
    if (!api) {
      return;
    }
    const pushChange = () => {
      const rawCards = api.getCards() as (ParsedProjectTask & {
        label?: string;
        tagsText?: string;
        usersText?: string;
      })[];
      // Reverse of the mapping below — Kanban only ever mutates `label`/`tagsText`/`usersText`.
      onChange(
        rawCards.map(({ label, tagsText, usersText, ...card }) => ({
          ...card,
          text: label ?? card.text,
          tags: splitCommaList(tagsText),
          users: splitCommaList(usersText),
        })),
      );
    };
    api.on("add-card", pushChange);
    api.on("update-card", pushChange);
    api.on("move-card", pushChange);
    api.on("delete-card", pushChange);
    // eslint-disable-next-line react-hooks/exhaustive-deps -- see GanttTab's identical comment.
  }, [api]);

  // Kanban re-seeds its whole store whenever the `columns`/`cards` prop identity changes (see its
  // `useEffect([cards, columns, ...])` re-init), so these must stay referentially stable across
  // renders that don't actually touch columns/tasks — otherwise every unrelated re-render (e.g. a
  // task edit while the columns panel is open) would blow away the board's scroll/collapsed state.
  const kanbanColumns = useMemo(
    () => columns.map((column) => ({ id: column.id, label: column.label })),
    [columns],
  );
  // KanbanCard's title field is `label`, not our schema's `text` — without this mapping the card
  // renders with no visible title at all (the board only ever reads `.label`). tagsText/usersText
  // are the flat, editable string form of our `tags`/`users` arrays (see kanbanEditorItems above).
  const cards = useMemo(
    () =>
      tasks.map((task) => ({
        ...task,
        label: task.text,
        tagsText: (task.tags ?? []).join(", "),
        usersText: (task.users ?? []).join(", "),
      })),
    [tasks],
  );

  return (
    // See GanttTab's identical comment on `fonts={false}` and `placement="inline"`.
    <KanbanTheme fonts={false}>
      <div className="project-board-tab-layout">
        <div className="project-board-tab-widget">
          <Kanban init={setApi} cards={cards} columns={kanbanColumns} card={kanbanCardShape} />
        </div>
        {/* Clicking a card dispatches select-card, which this picks up automatically. */}
        {api ? <KanbanEditor api={api} items={kanbanEditorItems} placement="inline" /> : null}
      </div>
    </KanbanTheme>
  );
}

const priorityLabelById = new Map(getPriorityOptions().map((option) => [option.id, option.label]));

function GridTab({ tasks }: { tasks: ParsedProjectTask[] }) {
  const columns = [
    { id: "text", header: "Task", width: 240 },
    { id: "column", header: "Status", width: 120 },
    { id: "priority", header: "Priority", width: 100 },
    { id: "start", header: "Start", width: 140 },
    { id: "duration", header: "Duration (days)", width: 140 },
    { id: "progress", header: "Progress %", width: 120 },
  ];
  // Grid renders whatever's in each cell as-is; format the Date back to a plain date string, and
  // the numeric priority level back to its label, so neither shows up as a raw value.
  const rows = tasks.map((task) => ({
    ...task,
    start: task.start.toLocaleDateString(),
    priority: task.priority !== undefined ? (priorityLabelById.get(task.priority) ?? "") : "",
  }));

  return (
    // See GanttTab's identical comment on `fonts={false}`.
    <GridTheme fonts={false}>
      <Grid data={rows} columns={columns} />
    </GridTheme>
  );
}

export function ProjectBoardView({
  bridge,
  pageId,
  editable,
  rawContent,
}: {
  bridge: EditorBridge | null;
  pageId: string;
  editable: boolean;
  rawContent: string | undefined;
}) {
  const [board, setBoard] = useState<ProjectBoard | null>(() => parseProjectBoardJson(rawContent ?? ""));
  const [parseFailed, setParseFailed] = useState(false);
  const [viewMode, setViewMode] = useState<ViewMode>("gantt");
  const [columnsPanelOpen, setColumnsPanelOpen] = useState(false);
  const snapshot_timer = useRef<number | null>(null);
  const loaded_page_id = useRef<string | null>(null);

  // Re-parse whenever a different document (or the same one reloaded) is handed to us — mirrors
  // NotebookView's identical reset-on-switch effect, including cancelling any debounced save
  // scheduled for the previous board so it can't fire against the newly switched-to document.
  useEffect(() => {
    if (loaded_page_id.current === pageId) {
      return;
    }
    loaded_page_id.current = pageId;
    if (snapshot_timer.current !== null) {
      window.clearTimeout(snapshot_timer.current);
      snapshot_timer.current = null;
    }
    const parsed = parseProjectBoardJson(rawContent ?? "");
    setBoard(parsed);
    setParseFailed(parsed === null);
  }, [pageId, rawContent]);

  useEffect(() => {
    return () => {
      if (snapshot_timer.current !== null) {
        window.clearTimeout(snapshot_timer.current);
      }
    };
  }, []);

  const scheduleSave = (next: ProjectBoard) => {
    if (!bridge || !editable) {
      return;
    }
    if (snapshot_timer.current !== null) {
      window.clearTimeout(snapshot_timer.current);
    }
    snapshot_timer.current = window.setTimeout(() => {
      snapshot_timer.current = null;
      void bridge.updateSnapshot(pageId, next);
    }, snapshotDebounceMs);
  };

  const handleTasksChange = (nextParsedTasks: ParsedProjectTask[]) => {
    if (!board) {
      return;
    }
    const next = { ...board, tasks: fromParsedTasks(nextParsedTasks) };
    setBoard(next);
    scheduleSave(next);
  };

  const handleAddTask = () => {
    const base = board ?? { tasks: [], columns: [] };
    const newTask: ProjectTask = {
      id: makeTaskId(),
      text: "New task",
      start: new Date().toISOString(),
      duration: 1,
      progress: 0,
      column: base.columns[0]?.id ?? "todo",
      type: "task",
    };
    const next = { ...base, tasks: [...base.tasks, newTask] };
    setBoard(next);
    scheduleSave(next);
  };

  const handleAddColumn = () => {
    const base = board ?? { tasks: [], columns: [] };
    const newColumn: ProjectColumn = { id: makeColumnId(), label: "New column" };
    const next = { ...base, columns: [...base.columns, newColumn] };
    setBoard(next);
    scheduleSave(next);
  };

  const handleRenameColumn = (columnId: string, label: string) => {
    if (!board) {
      return;
    }
    const next = {
      ...board,
      columns: board.columns.map((column) => (column.id === columnId ? { ...column, label } : column)),
    };
    setBoard(next);
    scheduleSave(next);
  };

  const handleDeleteColumn = (columnId: string) => {
    if (!board || board.columns.length <= 1) {
      return;
    }
    const remaining = board.columns.filter((column) => column.id !== columnId);
    const fallbackColumnId = remaining[0].id;
    const next = {
      columns: remaining,
      // Tasks orphaned by the deleted column move to the first remaining one instead of being
      // silently dropped from every view.
      tasks: board.tasks.map((task) =>
        task.column === columnId ? { ...task, column: fallbackColumnId } : task,
      ),
    };
    setBoard(next);
    scheduleSave(next);
  };

  // Both Gantt and Kanban fully reinitialize their internal store whenever the `tasks`/`cards`
  // prop identity changes (see KanbanTab's comment above) — recomputing this on every render,
  // even ones that don't touch task data (switching tabs, renaming a column, toggling the columns
  // panel), was forcing a hard reset on every such render. Worst case: a reset landing mid-drag,
  // which is what made a column appear to vanish while a task was being dragged. Memoize on
  // `board?.tasks` so identity only changes when the task data itself actually changed. This must
  // stay before the parseFailed early return below so the hook always runs (Rules of Hooks).
  const tasks = useMemo(() => toParsedTasks(board?.tasks ?? []), [board?.tasks]);
  const columns = board?.columns ?? [];

  if (parseFailed) {
    return (
      <div className="empty-state" data-testid="project-board-parse-error">
        <h1>Could not read project board</h1>
        <p>The stored document is not valid project board JSON.</p>
      </div>
    );
  }

  return (
    <div className="project-board-view" data-testid="project-board-view">
      <div className="project-board-toolbar">
        <div className="project-board-tabs">
          <button
            type="button"
            className={viewMode === "gantt" ? "project-board-tab--active" : undefined}
            onClick={() => setViewMode("gantt")}
          >
            Gantt
          </button>
          <button
            type="button"
            className={viewMode === "kanban" ? "project-board-tab--active" : undefined}
            onClick={() => setViewMode("kanban")}
          >
            Kanban
          </button>
          <button
            type="button"
            className={viewMode === "grid" ? "project-board-tab--active" : undefined}
            onClick={() => setViewMode("grid")}
          >
            Table
          </button>
        </div>
        {editable ? (
          <div className="project-board-toolbar-actions">
            <button
              type="button"
              className={columnsPanelOpen ? "project-board-tab--active" : undefined}
              onClick={() => setColumnsPanelOpen((open) => !open)}
            >
              Columns
            </button>
            <button type="button" onClick={handleAddTask}>
              + Task
            </button>
          </div>
        ) : null}
      </div>
      {editable && columnsPanelOpen ? (
        <div className="project-board-columns-panel" data-testid="project-board-columns-panel">
          <ul className="project-board-columns-list">
            {columns.map((column) => (
              <li key={column.id} className="project-board-columns-row">
                <input
                  type="text"
                  value={column.label}
                  onChange={(event) => handleRenameColumn(column.id, event.target.value)}
                />
                <button
                  type="button"
                  className="project-board-columns-remove"
                  onClick={() => handleDeleteColumn(column.id)}
                  disabled={columns.length <= 1}
                  aria-label={`Remove column ${column.label}`}
                >
                  ×
                </button>
              </li>
            ))}
          </ul>
          <button type="button" onClick={handleAddColumn}>
            + Add column
          </button>
        </div>
      ) : null}
      {/* Always render the actual board, even with zero tasks (an empty Kanban still shows its
          columns, an empty Gantt/Grid still shows their structure) — this used to swap the whole
          surface out for a plain "no tasks yet" placeholder whenever `tasks.length === 0`, which
          also hid the columns whenever that count dipped to zero even transiently (e.g. from a
          stale intermediate read while Kanban re-seeds its store after a drag), matching reports
          of a newly added column disappearing entirely. */}
      <div className="project-board-surface">
        {viewMode === "gantt" ? <GanttTab tasks={tasks} onChange={handleTasksChange} /> : null}
        {viewMode === "kanban" ? (
          <KanbanTab tasks={tasks} columns={columns} onChange={handleTasksChange} />
        ) : null}
        {viewMode === "grid" ? <GridTab tasks={tasks} /> : null}
      </div>
    </div>
  );
}
