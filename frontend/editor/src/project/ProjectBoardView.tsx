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
  type ProjectLink,
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

// Computed once (like kanbanEditorItems above), not inline in GanttTab's JSX — `getGanttEditorItems()`
// was being called fresh on every render there, handing <GanttEditor> a brand-new `items` array
// identity on every single edit even after Gantt's own `tasks`/`links` were frozen at mount (see
// GanttTab), which was still enough to make the whole widget flicker on every action.
const ganttEditorItems = getGanttEditorItems();

// A small icon-only Undo/Redo pair for the page toolbar. Earlier this called into each SVAR
// widget's own built-in history (`api.exec("undo"/"redo")`, `undo`/`history` config), but that
// turned out not to actually work: reading the compiled bundles, Gantt's `getHistory()` reads a
// "historyManager" module that's referenced by type only and never registered/instantiated
// anywhere in this (non-"pro") package, and Kanban's store calls `this._history.undo()` /
// `.pushUndo()` against a `_history` field that's likewise never assigned — both silently no-op
// (or would throw) rather than doing anything. Grid's own history looked genuinely implemented,
// but "undo works on one of three tabs, silently does nothing on the other two" is a worse
// experience than one undo history that actually works everywhere, so this is now a single
// app-level undo stack over `board` itself (see ProjectBoardView's history/handleUndo/handleRedo)
// instead of relying on any per-widget SVAR module. Plain Unicode glyphs, not SVAR's icon font
// (disabled everywhere via `fonts={false}` to avoid its external CDN request).
function UndoRedoButtons({
  onUndo,
  onRedo,
  canUndo,
  canRedo,
}: {
  onUndo: () => void;
  onRedo: () => void;
  canUndo: boolean;
  canRedo: boolean;
}) {
  return (
    <div className="project-board-undo-redo">
      <button
        type="button"
        className="project-board-icon-button"
        onClick={onUndo}
        disabled={!canUndo}
        title="Undo"
        aria-label="Undo"
      >
        ↶
      </button>
      <button
        type="button"
        className="project-board-icon-button"
        onClick={onRedo}
        disabled={!canRedo}
        title="Redo"
        aria-label="Redo"
      >
        ↷
      </button>
    </div>
  );
}

function GanttTab({
  tasks,
  onChange,
  links,
  onLinksChange,
  onApiReady,
}: {
  tasks: ParsedProjectTask[];
  onChange: (tasks: ParsedProjectTask[]) => void;
  links: ProjectLink[];
  onLinksChange: (links: ProjectLink[]) => void;
  onApiReady: (api: SvarApi | null) => void;
}) {
  // `useState` (not `useRef`) so mounting <GanttEditor> below re-renders once the api is ready —
  // it's null on the very first render, before Gantt's `init` callback fires post-mount.
  const [api, setApi] = useState<SvarApi>(null);

  useEffect(() => {
    onApiReady(api);
    return () => onApiReady(null);
    // eslint-disable-next-line react-hooks/exhaustive-deps -- see KanbanSwimlane's identical comment.
  }, [api]);

  // Gantt does the exact same hard-reset-on-prop-change thing Kanban does (see KanbanSwimlane's
  // comment) — feeding it a fresh `tasks`/`links` array on every single render (which is what
  // happens naturally, since every edit updates `board` and both are recomputed from it) meant
  // Gantt fully reinitialized its internal store after every single action, visible as a constant
  // flicker/reset of the whole chart. Freeze both at mount instead: ordinary edits (drag, resize,
  // the Editor) already land in Gantt's own store the moment they happen — there's nothing to feed
  // back in for those. The one thing that still needs pushing in from outside is a task added via
  // the page's "+ Task" button while this tab is showing (see handleAddTask's use of onApiReady
  // above); anything that changes `board` in a way too broad to replay as one action (Undo/Redo)
  // instead forces a full remount via a key change (see ProjectBoardView's historyVersion).
  const initialTasksRef = useRef(tasks);
  const initialLinksRef = useRef(links);
  const ganttTasks = useMemo(() => initialTasksRef.current, []);
  const ganttLinks = useMemo(() => initialLinksRef.current, []);

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

    // Dependency links: dragging between two task bars (or the Editor's own link controls, if any)
    // fires these three events; `serialize({data: "links"})` mirrors the `pushChange` pattern above
    // for tasks.
    const pushLinksChange = () => {
      const serialized = api.serialize({ data: "links" }) as ProjectLink[] | null;
      if (serialized) {
        onLinksChange(serialized);
      }
    };
    api.on("add-link", pushLinksChange);
    api.on("update-link", pushLinksChange);
    api.on("delete-link", pushLinksChange);
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
          {/* `links`: drag from a task bar's edge to another to create a dependency. */}
          <Gantt init={setApi} tasks={ganttTasks} links={ganttLinks} />
        </div>
        {/* Double-clicking a task opens this automatically (SVAR's built-in behavior) — no
            separate wiring needed beyond mounting it alongside Gantt with the same api. */}
        {api ? <GanttEditor api={api} items={ganttEditorItems} placement="inline" /> : null}
      </div>
    </GanttTheme>
  );
}

// One epic's own Kanban board — a full <Kanban> + inline editor instance scoped to just that
// epic's tasks, all sharing the same status `columns`. This is how "epics" are done here: SVAR
// Kanban has no native swimlane/grouping-into-lanes concept (confirmed against its docs/types —
// its own "grouping" feature only lets you swap the whole column set for a different one, not
// stack multiple lanes of the same columns), and every other maintained open-source React kanban
// library we found has the same gap; a real swimlane board with cross-lane drag would mean
// building our own drag-and-drop engine, cards, and edit form from scratch — a much bigger,
// separate undertaking. Stacking one full board per epic reuses everything already built here
// (cards, editing, in-column drag) at the cost of one thing: dragging a card to a DIFFERENT
// epic doesn't work (each instance's drag-and-drop is scoped to itself) — that's done instead via
// the "Epic" field in the edit form (see epicOptions/parent below), which updates the data
// immediately but only shows the card in its new lane after the Kanban tab is remounted (switch
// tabs and back, or reopen the document) — the same frozen-props tradeoff that fixed the
// disappearing-column bug applies per-lane here too.
function KanbanSwimlane({
  epicId,
  epicLabel,
  tasks,
  columns,
  epicOptions,
  onChange,
  onApiReady,
}: {
  epicId: string | null;
  epicLabel: string;
  tasks: ParsedProjectTask[];
  columns: ProjectColumn[];
  epicOptions: { id: string; label: string }[];
  onChange: (originalIds: Set<string>, updatedTasks: ParsedProjectTask[]) => void;
  onApiReady: (epicId: string | null, api: SvarApi | null) => void;
}) {
  // See GanttTab's identical comment on why this is useState, not useRef.
  const [api, setApi] = useState<SvarApi>(null);

  useEffect(() => {
    onApiReady(epicId, api);
    return () => onApiReady(epicId, null);
    // eslint-disable-next-line react-hooks/exhaustive-deps -- onApiReady/epicId are stable for
    // this instance's lifetime; re-running for parent re-renders would serve no purpose.
  }, [api]);

  // Which task ids this lane started with, frozen at mount — lets the change handler below merge
  // just this lane's edits back into the full shared task list without touching other lanes' data
  // (see ProjectBoardView's handleSwimlaneChange). A task moved OUT of this epic via the "Epic"
  // field still counts as "this lane's edit" (it's still in this set), same for one moved in via
  // handleAddTask while this lane is live.
  const initialTaskIdsRef = useRef(new Set(tasks.map((task) => task.id)));

  useEffect(() => {
    if (!api) {
      return;
    }
    const pushChange = () => {
      const cards = api.getCards() as (ParsedProjectTask & { label?: string })[];
      // Reverse of the `label: task.text` mapping below — Kanban only ever mutates `label`.
      onChange(
        initialTaskIdsRef.current,
        cards.map(({ label, ...card }) => ({ ...card, text: label ?? card.text })),
      );
    };
    api.on("add-card", pushChange);
    api.on("update-card", pushChange);
    api.on("move-card", pushChange);
    api.on("delete-card", pushChange);
    // eslint-disable-next-line react-hooks/exhaustive-deps -- see GanttTab's identical comment.
  }, [api]);

  // See the identical comment on KanbanTab's predecessor (still applies per-lane): Kanban fully
  // reinitializes its whole internal store whenever `cards`/`columns` prop identity changes, so
  // these are seeded once at mount and never recomputed from later prop changes.
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

  // The "Epic" field is a plain built-in richselect (not a custom field like TagListField) since
  // it just needs an options list — but that list depends on the board's current epics, so it's
  // computed fresh per render (safe: this only affects the Editor's `items` prop, not Kanban's own
  // `cards`/`columns`, so it doesn't trigger the store reinit described above).
  const editorItems = useMemo(
    () => [
      ...kanbanEditorItems,
      {
        comp: "richselect",
        key: "parent",
        label: "Epic",
        options: [{ id: "", label: "No epic" }, ...epicOptions],
      },
    ],
    [epicOptions],
  );

  return (
    <div className="project-board-swimlane">
      <div className="project-board-swimlane-label">{epicLabel}</div>
      {/* See GanttTab's identical comment on `fonts={false}` and `placement="inline"`. */}
      <KanbanTheme fonts={false}>
        <div className="project-board-tab-layout">
          <div className="project-board-tab-widget">
            <Kanban init={setApi} cards={cards} columns={kanbanColumns} card={kanbanCardShape} />
          </div>
          {/* Clicking a card dispatches select-card, which this picks up automatically. */}
          {api ? <KanbanEditor api={api} items={editorItems} placement="inline" /> : null}
        </div>
      </KanbanTheme>
    </div>
  );
}

// Epics are just tasks with `type: "summary"` — the same concept Gantt already uses for a
// parent/rollup task with children (via other tasks' `parent` field pointing at it). Reusing it
// here keeps "one shared task list" true across all three views instead of inventing a
// Kanban-only epic concept: a Gantt summary bar and a Kanban swimlane are the same underlying row.
const NO_EPIC_KEY = "__no_epic__";

function KanbanBoard({
  tasks,
  columns,
  onChange,
  onApiReady,
}: {
  tasks: ParsedProjectTask[];
  columns: ProjectColumn[];
  onChange: (originalIds: Set<string>, updatedTasks: ParsedProjectTask[]) => void;
  onApiReady: (epicId: string | null, api: SvarApi | null) => void;
}) {
  const epics = useMemo(() => tasks.filter((task) => task.type === "summary"), [tasks]);
  const epicOptions = useMemo(
    () => epics.map((epic) => ({ id: epic.id, label: epic.text })),
    [epics],
  );

  const lanes = useMemo(() => {
    const byEpic = new Map<string, ParsedProjectTask[]>();
    byEpic.set(NO_EPIC_KEY, []);
    for (const epic of epics) {
      byEpic.set(epic.id, []);
    }
    for (const task of tasks) {
      if (task.type === "summary") {
        continue;
      }
      const key = task.parent && byEpic.has(task.parent) ? task.parent : NO_EPIC_KEY;
      byEpic.get(key)!.push(task);
    }
    return [
      ...epics.map((epic) => ({ epicId: epic.id, label: epic.text, tasks: byEpic.get(epic.id)! })),
      { epicId: null, label: "No epic", tasks: byEpic.get(NO_EPIC_KEY)! },
    ];
  }, [tasks, epics]);

  return (
    <div className="project-board-swimlanes">
      {lanes.map((lane) => (
        <KanbanSwimlane
          key={lane.epicId ?? NO_EPIC_KEY}
          epicId={lane.epicId}
          epicLabel={lane.label}
          tasks={lane.tasks}
          columns={columns}
          epicOptions={epicOptions}
          onChange={onChange}
          onApiReady={onApiReady}
        />
      ))}
    </div>
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
  // Status pills cycle through a fixed small palette by the status's position in the board's
  // column list, so each status keeps a stable, distinct color (Notion-style tag) without needing
  // per-status color configuration anywhere in the schema.
  const columnToneById = useMemo(
    () => new Map(columns.map((column, index) => [column.id, index % 6])),
    [columns],
  );

  // Column-level `cell` renderers (a real React component, distinct from `template`'s plain-string
  // formatting — see @svar-ui/grid-store's getRenderValue vs. react-grid's `n.cell`) let Status/
  // Priority render as colored pills instead of raw text; `template` is kept alongside for the
  // plain-text fallback used by tooltips/export.
  const StatusCell = useMemo(
    () =>
      function StatusCell({ row }: { row: Record<string, any> }) {
        const value = row.column as string;
        const label = columnLabelById.get(value);
        // A task can end up pointing at a column id that no longer exists (e.g. a column deleted
        // out from under it in an older build) — show a neutral placeholder instead of leaking the
        // raw internal id (`column-<timestamp>-<n>`) onto the screen.
        if (label === undefined) {
          return <span className="project-board-pill project-board-pill--unknown">Unassigned</span>;
        }
        const tone = columnToneById.get(value) ?? 0;
        return <span className={`project-board-pill project-board-pill--tone-${tone}`}>{label}</span>;
      },
    [columnLabelById, columnToneById],
  );
  const PriorityCell = useMemo(
    () =>
      function PriorityCell({ row }: { row: Record<string, any> }) {
        const value = row.priority as number | undefined;
        if (value === undefined) {
          return null;
        }
        const tone = value === 3 ? "high" : value === 2 ? "medium" : "low";
        return (
          <span className={`project-board-pill project-board-pill--priority-${tone}`}>
            {priorityLabelById.get(value) ?? ""}
          </span>
        );
      },
    [],
  );

  // Sortable by clicking a header, and editable in place (status/priority via a dropdown of the
  // board's actual columns/priority levels, dates via a real date picker) — using what the Grid
  // already provides instead of a plain read-only table.
  const gridColumns = [
    { id: "text", header: "Task", width: 240, sort: true, editor: "text" },
    {
      id: "column",
      header: "Status",
      width: 150,
      sort: true,
      editor: { type: "richselect", config: { options: columnOptions } },
      template: (value: string) => columnLabelById.get(value) ?? value,
      cell: StatusCell,
    },
    {
      id: "priority",
      header: "Priority",
      width: 130,
      sort: true,
      editor: { type: "richselect", config: { options: priorityOptions } },
      template: (value: number | undefined) => (value !== undefined ? (priorityLabelById.get(value) ?? "") : ""),
      cell: PriorityCell,
    },
    {
      id: "start",
      header: "Start",
      width: 150,
      sort: true,
      editor: "datepicker",
      // Grid's default display value is the raw Date's `toString()` (e.g. "Fri Jul 24 2026
      // 20:00:00 GMT+0300 (MSK)"), which is both unreadable and gets clipped by the column width.
      template: (value: Date | undefined) =>
        value instanceof Date
          ? value.toLocaleDateString(undefined, { year: "numeric", month: "short", day: "numeric" })
          : "",
    },
    { id: "duration", header: "Duration (days)", width: 150, sort: true, editor: "text" },
    // `flexgrow` lets this last column absorb any leftover width so the row width matches the
    // header row's — without it the header's full-bleed background extends past where the data
    // rows actually end, which read as a stray highlighted block to the right of the table.
    { id: "progress", header: "Progress %", width: 120, sort: true, flexgrow: 1 },
  ];

  return (
    // See GanttTab's identical comment on `fonts={false}`.
    <GridTheme fonts={false}>
      <div className="project-board-grid-wrap">
        <Grid
          init={setApi}
          data={tasks}
          columns={gridColumns}
          sizes={{ rowHeight: 40, headerHeight: 38 }}
        />
      </div>
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
  // One live Kanban api per epic swimlane (keyed by epic id, or NO_EPIC_KEY for the "No epic"
  // lane), populated by KanbanSwimlane's onApiReady while the Kanban tab is showing; empty
  // otherwise. Add/rename/delete-column push into every lane at once (columns are shared across
  // all of them); add-task pushes into just the lane matching the new task's epic — see
  // KanbanSwimlane's comment on why changing `cards`/`columns` after mount is unsafe.
  const kanbanApisRef = useRef<Map<string, SvarApi>>(new Map());
  // Gantt has exactly one instance mounted at a time (unlike Kanban's per-epic map above), so a
  // single slot is enough — used only to push a task added via "+ Task" straight into Gantt's live
  // store while its tab is showing (see GanttTab's comment on why its `tasks`/`links` are otherwise
  // frozen at mount and don't take further pushes through props).
  const ganttApiRef = useRef<SvarApi | null>(null);
  // A single, document-wide undo/redo history over `board` itself — see UndoRedoButtons' comment
  // for why this replaced trying to use each SVAR widget's own (partly broken) history module.
  // `past`/`future` hold full prior/subsequent `ProjectBoard` snapshots, oldest-adjacent-to-current
  // first, so `past[past.length - 1]` / `future[0]` are always the one step away from `board`.
  const [past, setPast] = useState<ProjectBoard[]>([]);
  const [future, setFuture] = useState<ProjectBoard[]>([]);
  // Bumped only by Undo/Redo (see handleUndo/handleRedo) — folded into the Gantt/Kanban tabs' `key`
  // below to force a full remount specifically then. Both freeze their task/card data at mount
  // (see GanttTab/KanbanSwimlane) to stop SVAR's own reinit-on-every-edit flicker, which means an
  // arbitrary Undo/Redo (not a single replayable action, unlike a normal edit) can only actually
  // reach the screen through a deliberate remount — an occasional, expected refresh on an explicit
  // "jump to a different state" action, not the same thing as flickering on every routine edit.
  const [historyVersion, setHistoryVersion] = useState(0);

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
    // A history entry pointing at a different document's board would let "Undo" jump the user
    // back into a document they've since navigated away from — start fresh each time.
    setPast([]);
    setFuture([]);
    setHistoryVersion(0);
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

  // Every board mutation in this component funnels through here instead of calling
  // `setBoard`/`scheduleSave` directly, so the undo history stays complete regardless of which
  // view (or which control within it) produced the change.
  const commitBoard = (next: ProjectBoard) => {
    if (board) {
      setPast((p) => [...p, board]);
    }
    setFuture([]);
    setBoard(next);
    scheduleSave(next);
  };

  const handleUndo = () => {
    if (past.length === 0 || !board) {
      return;
    }
    const previous = past[past.length - 1];
    setPast(past.slice(0, -1));
    setFuture((f) => [board, ...f]);
    setBoard(previous);
    scheduleSave(previous);
    setHistoryVersion((v) => v + 1);
  };

  const handleRedo = () => {
    if (future.length === 0 || !board) {
      return;
    }
    const [next, ...rest] = future;
    setFuture(rest);
    setPast((p) => [...p, board]);
    setBoard(next);
    scheduleSave(next);
    setHistoryVersion((v) => v + 1);
  };

  const handleTasksChange = (nextParsedTasks: ParsedProjectTask[]) => {
    if (!board) {
      return;
    }
    commitBoard({ ...board, tasks: fromParsedTasks(nextParsedTasks) });
  };

  // Merges one swimlane's edits back into the full shared task list — `originalIds` is the set of
  // task ids that lane started with (frozen at its mount), so this replaces exactly those and
  // leaves every other lane's tasks untouched, regardless of adds/edits/deletes/moves within it.
  const handleSwimlaneChange = (originalIds: Set<string>, updatedSubset: ParsedProjectTask[]) => {
    if (!board) {
      return;
    }
    const untouched = board.tasks.filter((task) => !originalIds.has(task.id));
    commitBoard({ ...board, tasks: [...untouched, ...fromParsedTasks(updatedSubset)] });
  };

  // Dependency links are only ever drawn/edited from the Gantt view (see GanttTab) — this just
  // persists whatever the full links array looks like after each add/update/delete.
  const handleLinksChange = (nextLinks: ProjectLink[]) => {
    if (!board) {
      return;
    }
    commitBoard({ ...board, links: nextLinks });
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
    commitBoard(next);
    // The new task has no epic (`parent` unset), so it belongs to the "No epic" lane — push it
    // into that lane's live api directly if the Kanban tab is showing (its `cards` prop is frozen
    // after mount, see KanbanSwimlane, so the state update above alone wouldn't show up there).
    const noEpicApi = kanbanApisRef.current.get(NO_EPIC_KEY);
    if (viewMode === "kanban" && noEpicApi) {
      void noEpicApi.exec("add-card", { card: { ...newTask, label: newTask.text } });
    }
    // Same story for Gantt — its `tasks` are frozen at mount too (see GanttTab), so a task added
    // this way needs pushing into its live store directly, with `start` hydrated to a real Date
    // (Gantt needs one to place the bar; `newTask` above only has the ISO string form).
    if (viewMode === "gantt" && ganttApiRef.current) {
      void ganttApiRef.current.exec("add-task", { task: toParsedTasks([newTask])[0] });
    }
  };

  const handleAddEpic = () => {
    const base = board ?? { tasks: [], columns: [] };
    const newEpic: ProjectTask = {
      id: makeTaskId(),
      text: "New epic",
      start: new Date().toISOString(),
      duration: 1,
      progress: 0,
      column: base.columns[0]?.id ?? "",
      type: "summary",
    };
    const next = { ...base, tasks: [...base.tasks, newEpic] };
    commitBoard(next);
    // A new epic means a whole new swimlane, which no live api can add on its own — the Kanban
    // tab just needs a remount to pick it up (switching to it does this naturally via `tasks`
    // changing; if it's already the active tab, force it by dropping all live refs so the render
    // below re-derives `lanes` and mounts a fresh <KanbanSwimlane> for it).
    kanbanApisRef.current.clear();
  };

  const handleAddColumn = () => {
    const base = board ?? { tasks: [], columns: [] };
    const newColumn: ProjectColumn = { id: makeColumnId(), label: "New column" };
    const nextColumns = [...base.columns, newColumn];
    const next = { ...base, columns: nextColumns };
    commitBoard(next);
    // Columns are shared across every epic's lane — patch all of them, not just one.
    for (const api of kanbanApisRef.current.values()) {
      // No public "add-column" store action exists (only "update-column"); patch the store's
      // own state directly instead of going through the `columns` prop, which is frozen after
      // mount for the same reason `cards` is (see KanbanSwimlane) — changing it would force a
      // full store reinit rather than a live update.
      api
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
    commitBoard(next);
    for (const api of kanbanApisRef.current.values()) {
      void api.exec("update-column", { id: columnId, column: { label } });
    }
  };

  const handleDeleteColumn = (columnId: string) => {
    if (!board || board.columns.length <= 1) {
      return;
    }
    const remaining = board.columns.filter((column) => column.id !== columnId);
    const fallbackColumnId = remaining[0].id;
    const next = {
      ...board,
      columns: remaining,
      // Tasks orphaned by the deleted column move to the first remaining one instead of being
      // silently dropped from every view.
      tasks: board.tasks.map((task) =>
        task.column === columnId ? { ...task, column: fallbackColumnId } : task,
      ),
    };
    commitBoard(next);
    const affectedTaskIds = board.tasks.filter((task) => task.column === columnId).map((task) => task.id);
    // Each affected task only lives in one lane's api, but we don't track that mapping here — a
    // "move-card" call against a lane that doesn't have the card is expected to just no-op, so
    // broadcasting to every lane is simpler and safe (not independently verified live).
    for (const api of kanbanApisRef.current.values()) {
      api
        .getStores()
        .data.setState({ columns: remaining.map((column) => ({ id: column.id, label: column.label })) });
      for (const taskId of affectedTaskIds) {
        void api.exec("move-card", { id: taskId, column: fallbackColumnId });
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
  const links = useMemo(() => board?.links ?? [], [board?.links]);

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
            {/* One history for the whole document, regardless of which tab produced the edit —
                see the comment on UndoRedoButtons for why this isn't per-widget. */}
            <UndoRedoButtons
              onUndo={handleUndo}
              onRedo={handleRedo}
              canUndo={past.length > 0}
              canRedo={future.length > 0}
            />
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
            {viewMode === "kanban" ? (
              <button type="button" onClick={handleAddEpic}>
                + Epic
              </button>
            ) : null}
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
      {/* `key`: both Gantt and every Kanban swimlane freeze their initial tasks/cards/columns at
          mount (see GanttTab/KanbanSwimlane's comments) — without a `pageId` component, switching
          to a different document while staying on the same tab would leave it seeded from the
          previous document instead of remounting fresh. The `historyVersion` component forces the
          same kind of remount specifically for Undo/Redo, since restoring an arbitrary past board
          state isn't something that can be pushed in as a single replayable action the way a
          normal edit can (see handleUndo/handleRedo). */}
      <div className="project-board-surface">
        {viewMode === "gantt" ? (
          <GanttTab
            key={`${pageId}:${historyVersion}`}
            tasks={tasks}
            onChange={handleTasksChange}
            links={links}
            onLinksChange={handleLinksChange}
            onApiReady={(api) => {
              ganttApiRef.current = api;
            }}
          />
        ) : null}
        {viewMode === "kanban" ? (
          <KanbanBoard
            key={`${pageId}:${historyVersion}`}
            tasks={tasks}
            columns={columns}
            onChange={handleSwimlaneChange}
            onApiReady={(epicId, api) => {
              const key = epicId ?? NO_EPIC_KEY;
              if (api) {
                kanbanApisRef.current.set(key, api);
              } else {
                kanbanApisRef.current.delete(key);
              }
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
