// Project board document kind (ADR-017 follow-up, issue #106): a single shared task list
// rendered through three switchable views (Gantt/Kanban/DataGrid), all backed by SVAR's
// open-source (MIT) React component suite (svar.dev). All three read/write the SAME
// `tasks`/`columns` arrays (see ./projectBoard.ts) — editing a task's dates in Gantt, or
// dragging its card to a different Kanban column, is the same underlying data as a row in the
// DataGrid view. Only one tab is mounted at a time (conditional rendering below), so switching
// tabs always remounts the next view with the latest shared state rather than needing live
// cross-component sync while multiple are mounted simultaneously.
import { useEffect, useRef, useState } from "react";

import { Gantt, WillowDark as GanttTheme } from "@svar-ui/react-gantt";
import "@svar-ui/react-gantt/all.css";
import { Kanban, WillowDark as KanbanTheme } from "@svar-ui/react-kanban";
import "@svar-ui/react-kanban/all.css";
import { Grid, WillowDark as GridTheme } from "@svar-ui/react-grid";
import "@svar-ui/react-grid/all.css";

import type { EditorBridge } from "../bridge/editorBridge";
import { snapshotDebounceMs } from "../constants";
import {
  fromParsedTasks,
  makeTaskId,
  parseProjectBoardJson,
  toParsedTasks,
  type ParsedProjectTask,
  type ProjectBoard,
  type ProjectColumn,
  type ProjectTask,
} from "./projectBoard";

type ViewMode = "gantt" | "kanban" | "grid";

function GanttTab({
  tasks,
  onChange,
}: {
  tasks: ParsedProjectTask[];
  onChange: (tasks: ParsedProjectTask[]) => void;
}) {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any -- SVAR's IApi type is generic
  // over its internal store action config; typing it precisely here isn't worth the friction.
  const apiRef = useRef<any>(null);

  useEffect(() => {
    const api = apiRef.current;
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
    // eslint-disable-next-line react-hooks/exhaustive-deps -- apiRef.current is stable once set;
    // re-running this on every `tasks`/`onChange` change would attach duplicate listeners.
  }, []);

  return (
    <GanttTheme>
      <Gantt ref={apiRef} tasks={tasks} />
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
  // eslint-disable-next-line @typescript-eslint/no-explicit-any -- see GanttTab's apiRef.
  const apiRef = useRef<any>(null);

  useEffect(() => {
    const api = apiRef.current;
    if (!api) {
      return;
    }
    const pushChange = () => {
      onChange(api.getCards() as ParsedProjectTask[]);
    };
    api.on("add-card", pushChange);
    api.on("update-card", pushChange);
    api.on("move-card", pushChange);
    api.on("delete-card", pushChange);
    // eslint-disable-next-line react-hooks/exhaustive-deps -- see GanttTab's identical comment.
  }, []);

  const kanbanColumns = columns.map((column) => ({ id: column.id, label: column.label }));

  return (
    <KanbanTheme>
      <Kanban ref={apiRef} cards={tasks} columns={kanbanColumns} />
    </KanbanTheme>
  );
}

function GridTab({ tasks }: { tasks: ParsedProjectTask[] }) {
  const columns = [
    { id: "text", header: "Task", width: 240 },
    { id: "column", header: "Status", width: 120 },
    { id: "start", header: "Start", width: 140 },
    { id: "duration", header: "Duration (days)", width: 140 },
    { id: "progress", header: "Progress %", width: 120 },
  ];
  // Grid renders whatever's in each cell as-is; format the Date back to a plain date string so
  // it doesn't show up as a verbose Date#toString() in the table.
  const rows = tasks.map((task) => ({ ...task, start: task.start.toLocaleDateString() }));

  return (
    <GridTheme>
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

  if (parseFailed) {
    return (
      <div className="empty-state" data-testid="project-board-parse-error">
        <h1>Could not read project board</h1>
        <p>The stored document is not valid project board JSON.</p>
      </div>
    );
  }

  const tasks = toParsedTasks(board?.tasks ?? []);
  const columns = board?.columns ?? [];

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
          <button type="button" onClick={handleAddTask}>
            + Task
          </button>
        ) : null}
      </div>
      {tasks.length === 0 ? (
        <div className="project-board-empty">
          <p>This project board has no tasks yet.</p>
        </div>
      ) : (
        <div className="project-board-surface">
          {viewMode === "gantt" ? <GanttTab tasks={tasks} onChange={handleTasksChange} /> : null}
          {viewMode === "kanban" ? (
            <KanbanTab tasks={tasks} columns={columns} onChange={handleTasksChange} />
          ) : null}
          {viewMode === "grid" ? <GridTab tasks={tasks} /> : null}
        </div>
      )}
    </div>
  );
}
