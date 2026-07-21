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
};

export type ProjectColumn = {
  id: string;
  label: string;
};

export type ProjectBoard = {
  tasks: ProjectTask[];
  columns: ProjectColumn[];
};

// Dates hydrated to real Date instances — the shape @svar-ui/react-gantt's ITask actually wants.
export type ParsedProjectTask = Omit<ProjectTask, "start" | "end"> & {
  start: Date;
  end?: Date;
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
  }));
}

export function fromParsedTasks(tasks: ParsedProjectTask[]): ProjectTask[] {
  return tasks.map((task) => ({
    ...task,
    start: task.start.toISOString(),
    end: task.end ? task.end.toISOString() : undefined,
  }));
}

let nextTaskSuffix = 0;
export function makeTaskId(): ProjectTaskId {
  nextTaskSuffix += 1;
  return `task-${Date.now()}-${nextTaskSuffix}`;
}
