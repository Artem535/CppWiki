// Project board document kind (ADR-017 follow-up): a single shared task list rendered through
// three switchable views (Gantt/Kanban/DataGrid, all from SVAR's open-source component suite —
// see ProjectBoardView.tsx). All three views read/write this one schema; nothing here is
// specific to any one SVAR component's own internal types.
export type ProjectTaskId = string;

export type ProjectTask = {
  id: ProjectTaskId;
  text: string;
  // ISO date strings in storage (see ParsedProjectTask for the Date-hydrated form the Gantt/
  // Calendar-shaped SVAR components expect at render time).
  start: string;
  duration: number;
  end?: string;
  progress: number;
  column: string;
  parent?: string;
  type?: "task" | "summary" | "milestone";
  tags?: string[];
  users?: string[];
  // Kanban's built-in priority levels (1 = Low, 2 = Medium, 3 = High), each mapped to a color —
  // this is the "card color" knob exposed in the Kanban card/editor (see ProjectBoardView.tsx).
  priority?: number;
  description?: string;
  // ISO date string in storage, like start/end — see ParsedProjectTask.
  deadline?: string;
};

export type ProjectColumn = {
  id: string;
  label: string;
};

// A Gantt task dependency (drawn/edited only from the Gantt view — Kanban/Grid don't visualize
// these). `type` follows the standard scheduling notation: "s2s"/"s2e"/"e2s"/"e2e" = start-to-start
// / start-to-end / end-to-start / end-to-end; "e2s" (finish-to-start) is the common case.
export type ProjectLinkType = "s2s" | "s2e" | "e2s" | "e2e";

export type ProjectLink = {
  id: string;
  type: ProjectLinkType;
  source: ProjectTaskId;
  target: ProjectTaskId;
  lag?: number;
};

export type ProjectBoard = {
  tasks: ProjectTask[];
  columns: ProjectColumn[];
  // Optional: documents saved before dependency links existed won't have this field.
  links?: ProjectLink[];
};

// Dates hydrated to real Date instances — the shape @svar-ui/react-gantt's ITask actually wants,
// and what Kanban's own datepicker editor field produces for `deadline`.
export type ParsedProjectTask = Omit<ProjectTask, "start" | "end" | "deadline"> & {
  start: Date;
  end?: Date;
  deadline?: Date;
};

const defaultColumns: ProjectColumn[] = [
  { id: "todo", label: "To do" },
  { id: "inProgress", label: "In progress" },
  { id: "done", label: "Done" },
];

export function parseProjectBoardJson(rawContent: string): ProjectBoard | null {
  if (!rawContent.trim()) {
    return { tasks: [], columns: defaultColumns };
  }
  try {
    const parsed: unknown = JSON.parse(rawContent);
    if (
      typeof parsed !== "object" ||
      parsed === null ||
      !Array.isArray((parsed as ProjectBoard).tasks) ||
      !Array.isArray((parsed as ProjectBoard).columns)
    ) {
      return null;
    }
    return parsed as ProjectBoard;
  } catch {
    return null;
  }
}

export function toParsedTasks(tasks: ProjectTask[]): ParsedProjectTask[] {
  return tasks.map((task) => ({
    ...task,
    start: new Date(task.start),
    end: task.end ? new Date(task.end) : undefined,
    deadline: task.deadline ? new Date(task.deadline) : undefined,
  }));
}

export function fromParsedTasks(tasks: ParsedProjectTask[]): ProjectTask[] {
  return tasks.map((task) => ({
    ...task,
    start: task.start.toISOString(),
    end: task.end ? task.end.toISOString() : undefined,
    deadline: task.deadline ? task.deadline.toISOString() : undefined,
  }));
}

let nextTaskSuffix = 0;
export function makeTaskId(): ProjectTaskId {
  nextTaskSuffix += 1;
  return `task-${Date.now()}-${nextTaskSuffix}`;
}

let nextColumnSuffix = 0;
export function makeColumnId(): string {
  nextColumnSuffix += 1;
  return `column-${Date.now()}-${nextColumnSuffix}`;
}
