// Project board document kind (ADR-017 follow-up, issue #106): a single shared task list
// rendered through three switchable views (Gantt/Kanban/DataGrid), all backed by SVAR's
// open-source (MIT) React component suite (svar.dev). All three read/write the SAME
// `tasks`/`columns` arrays (see ./projectBoard.ts) — editing a task's dates in Gantt, or
// dragging its card to a different Kanban column, is the same underlying data as a row in the
// DataGrid view. Only one tab is mounted at a time (conditional rendering below), so switching
// tabs always remounts the next view with the latest shared state rather than needing live
// cross-component sync while multiple are mounted simultaneously.
import { useEffect, useMemo, useRef, useState, type KeyboardEvent } from "react";

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
// tags/users: `true` (not an options object) is enough for the card *face* — its tag/avatar
// renderer resolves each raw string against the (absent) options list, and falls back to
// displaying the string itself when no match is found, so plain free-text values show up fine
// with no predefined vocabulary needed. The *editor* still needs its own, separate shape with
// these two off (see kanbanEditorItems) — its built-in multicombo field, unlike the card
// renderer, only lets you pick from a fixed options list with no "type a new one" support, which
// is why TagListField (a small field we own) replaces it there instead.
const kanbanCardShape = {
  priority: { data: getPriorityOptions() },
  progress: true,
  description: true,
  deadline: true,
  tags: true,
  users: true,
};

// A custom Editor field component (passed directly as `comp`, see kanbanEditorItems — SVAR's
// items config accepts a component here just as readily as one of its built-in type names). The
// surrounding <Field> wrapper already renders the item's `label` as a heading above this, and
// calls this with `value`/`onChange` bound to the raw array field named by the item's `key`
// (`tags` or `users`) — see react-editor's Field implementation.
function TagListField({
  value,
  onChange,
}: {
  value?: string[];
  onChange?: (ev: { value: string[] }) => void;
}) {
  const items = value ?? [];
  const [draft, setDraft] = useState<string | null>(null);

  const commitDraft = () => {
    const trimmed = draft?.trim();
    if (trimmed) {
      onChange?.({ value: [...items, trimmed] });
    }
    setDraft(null);
  };

  return (
    <div className="project-board-tag-list">
      {items.map((item, index) => (
        <span key={`${item}-${index}`} className="project-board-tag-chip">
          {item}
          <button
            type="button"
            onClick={() => onChange?.({ value: items.filter((_, i) => i !== index) })}
            aria-label={`Remove ${item}`}
          >
            ×
          </button>
        </span>
      ))}
      {draft !== null ? (
        <input
          autoFocus
          className="project-board-tag-input"
          value={draft}
          onChange={(event) => setDraft(event.target.value)}
          onBlur={commitDraft}
          onKeyDown={(event: KeyboardEvent<HTMLInputElement>) => {
            if (event.key === "Enter") {
              event.preventDefault();
              commitDraft();
            } else if (event.key === "Escape") {
              setDraft(null);
            }
          }}
        />
      ) : (
        <button type="button" className="project-board-tag-add" onClick={() => setDraft("")}>
          + Add
        </button>
      )}
    </div>
  );
}

// tags/users off here (unlike kanbanCardShape) so getKanbanEditorItems doesn't also add its own
// broken built-in multicombo item alongside TagListField's replacement below.
const kanbanEditorItems = [
  ...getKanbanEditorItems({ ...kanbanCardShape, tags: false, users: false }),
  { comp: TagListField, key: "tags", label: "Tags" },
  // Assignees are plain free-text names for now — this app has no user directory yet (planned to
  // come from Authentik later); the point right now is just to let a name be attached to a task.
  { comp: TagListField, key: "users", label: "Assignees" },
];

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
  onApiReady,
}: {
  tasks: ParsedProjectTask[];
  columns: ProjectColumn[];
  onChange: (tasks: ParsedProjectTask[]) => void;
  onApiReady: (api: SvarApi | null) => void;
}) {
  // See GanttTab's identical comment on why this is useState, not useRef.
  const [api, setApi] = useState<SvarApi>(null);

  useEffect(() => {
    onApiReady(api);
    return () => onApiReady(null);
    // eslint-disable-next-line react-hooks/exhaustive-deps -- onApiReady is a stable ref setter
    // from the parent; re-running this for identity changes there would serve no purpose.
  }, [api]);

  useEffect(() => {
    if (!api) {
      return;
    }
    const pushChange = () => {
      const cards = api.getCards() as (ParsedProjectTask & { label?: string })[];
      // Reverse of the `label: task.text` mapping below — Kanban only ever mutates `label`.
      onChange(cards.map(({ label, ...card }) => ({ ...card, text: label ?? card.text })));
    };
    api.on("add-card", pushChange);
    api.on("update-card", pushChange);
    api.on("move-card", pushChange);
    api.on("delete-card", pushChange);
    // eslint-disable-next-line react-hooks/exhaustive-deps -- see GanttTab's identical comment.
  }, [api]);

  // Kanban fully reinitializes its whole internal store whenever the `cards`/`columns` prop
  // identity changes (a hard reset, not a merge) — recomputing these from `tasks`/`columns` on
  // every render, even for edits Kanban's own store already knows about (its own `onChange` echo
  // above feeding back into a changed `tasks` prop), was forcing repeated resets, one of which
  // landing mid-drag is what made columns/cards vanish. So: seed ONLY from the props Kanban had
  // at mount time (frozen via ref, ignoring all later prop changes), and after that route every
  // mutation through the live api instead — Kanban's own interactions already do this natively;
  // ours (the parent's Add Task / Columns panel) do it explicitly via `onApiReady`'s ref, see
  // ProjectBoardView's handlers. The parent also keys this whole tab on `pageId` so switching to
  // a different document still gets a clean remount instead of a stale frozen seed.
  const initialTasksRef = useRef(tasks);
  const initialColumnsRef = useRef(columns);
  const kanbanColumns = useMemo(
    () => initialColumnsRef.current.map((column) => ({ id: column.id, label: column.label })),
    [],
  );
  const cards = useMemo(
    () => initialTasksRef.current.map((task) => ({ ...task, label: task.text })),
    [],
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

const priorityOptions = getPriorityOptions();
const priorityLabelById = new Map(priorityOptions.map((option) => [option.id, option.label]));

function GridTab({
  tasks,
  columns,
  onChange,
}: {
  tasks: ParsedProjectTask[];
  columns: ProjectColumn[];
  onChange: (tasks: ParsedProjectTask[]) => void;
}) {
  const [api, setApi] = useState<SvarApi>(null);

  useEffect(() => {
    if (!api) {
      return;
    }
    // `duration`/`priority` come back as strings/numbers from their editors (a plain text input
    // and a richselect keyed by numeric id, respectively) — coerce those two, everything else
    // (text, the status/column richselect, the datepicker's Date) already matches its field type.
    const pushChange = (ev: { id: string; column: string; value: string | number | Date }) => {
      onChange(
        tasks.map((task) => {
          if (task.id !== ev.id) {
            return task;
          }
          if (ev.column === "duration") {
            return { ...task, duration: Number(ev.value) || 0 };
          }
          if (ev.column === "priority") {
            return { ...task, priority: Number(ev.value) };
          }
          return { ...task, [ev.column]: ev.value };
        }),
      );
    };
    api.on("update-cell", pushChange);
    // eslint-disable-next-line react-hooks/exhaustive-deps -- see GanttTab's identical comment.
  }, [api]);

  const columnOptions = useMemo(
    () => columns.map((column) => ({ id: column.id, label: column.label })),
    [columns],
  );
  const columnLabelById = useMemo(() => new Map(columns.map((c) => [c.id, c.label])), [columns]);

  // Sortable by clicking a header, and editable in place (status/priority via a dropdown of the
  // board's actual columns/priority levels, dates via a real date picker) — using what the Grid
  // already provides instead of a plain read-only table.
  const gridColumns = [
    { id: "text", header: "Task", width: 240, sort: true, editor: "text" },
    {
      id: "column",
      header: "Status",
      width: 140,
      sort: true,
      editor: { type: "richselect", config: { options: columnOptions } },
      template: (value: string) => columnLabelById.get(value) ?? value,
    },
    {
      id: "priority",
      header: "Priority",
      width: 120,
      sort: true,
      editor: { type: "richselect", config: { options: priorityOptions } },
      template: (value: number | undefined) => (value !== undefined ? (priorityLabelById.get(value) ?? "") : ""),
    },
    { id: "start", header: "Start", width: 140, sort: true, editor: "datepicker" },
    { id: "duration", header: "Duration (days)", width: 140, sort: true, editor: "text" },
    { id: "progress", header: "Progress %", width: 120, sort: true },
  ];

  return (
    // See GanttTab's identical comment on `fonts={false}`.
    <GridTheme fonts={false}>
      <Grid init={setApi} data={tasks} columns={gridColumns} />
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
  // Set by KanbanTab's onApiReady while it's mounted and its Kanban instance is ready; null
  // otherwise (a different tab is showing, or the page just switched). Add/rename/delete-column
  // and add-task push through this directly when it's live, instead of through a changed prop —
  // see KanbanTab's comment on why changing `cards`/`columns` after mount is unsafe.
  const kanbanApiRef = useRef<SvarApi | null>(null);

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
    if (viewMode === "kanban" && kanbanApiRef.current) {
      // Kanban's `cards` prop is frozen after mount (see KanbanTab) — the state update above
      // alone wouldn't show up in the already-live board, so push it in directly too. The
      // resulting "add-card" event is a no-op for `board` (same data, already applied above).
      void kanbanApiRef.current.exec("add-card", { card: { ...newTask, label: newTask.text } });
    }
  };

  const handleAddColumn = () => {
    const base = board ?? { tasks: [], columns: [] };
    const newColumn: ProjectColumn = { id: makeColumnId(), label: "New column" };
    const nextColumns = [...base.columns, newColumn];
    const next = { ...base, columns: nextColumns };
    setBoard(next);
    scheduleSave(next);
    if (kanbanApiRef.current) {
      // No public "add-column" store action exists (only "update-column"); patch the store's
      // own state directly instead of going through the `columns` prop, which is frozen after
      // mount for the same reason `cards` is (see KanbanTab) — changing it would force a full
      // store reinit rather than a live update.
      kanbanApiRef.current
        .getStores()
        .data.setState({ columns: nextColumns.map((column) => ({ id: column.id, label: column.label })) });
    }
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
    if (kanbanApiRef.current) {
      void kanbanApiRef.current.exec("update-column", { id: columnId, column: { label } });
    }
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
    if (kanbanApiRef.current) {
      const api = kanbanApiRef.current;
      api
        .getStores()
        .data.setState({ columns: remaining.map((column) => ({ id: column.id, label: column.label })) });
      for (const task of board.tasks) {
        if (task.column === columnId) {
          void api.exec("move-card", { id: task.id, column: fallbackColumnId });
        }
      }
    }
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
      {/* `key={pageId}`: KanbanTab freezes its initial cards/columns at mount (see its comment) —
          without this, switching to a different document while staying on the Kanban tab would
          leave it seeded from the previous document instead of remounting fresh. */}
      <div className="project-board-surface">
        {viewMode === "gantt" ? (
          <GanttTab key={pageId} tasks={tasks} onChange={handleTasksChange} />
        ) : null}
        {viewMode === "kanban" ? (
          <KanbanTab
            key={pageId}
            tasks={tasks}
            columns={columns}
            onChange={handleTasksChange}
            onApiReady={(api) => {
              kanbanApiRef.current = api;
            }}
          />
        ) : null}
        {viewMode === "grid" ? (
          <GridTab tasks={tasks} columns={columns} onChange={handleTasksChange} />
        ) : null}
      </div>
    </div>
  );
}
