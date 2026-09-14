import { describe, expect, it } from "vitest";
import {
  colorForTagValue,
  formatPropertyValue,
  getPropertySummary,
  removePagePropertyValue,
  tagPaletteNames,
  upsertPagePropertyValue,
  type PagePropertyValue,
  type PropertyDefinition,
} from "./propertyModel";

const definition: PropertyDefinition = {
  id: "status",
  workspaceId: "engineering",
  name: "Status",
  valueKind: "select",
  options: ["Draft", "Approved"],
  state: "active",
};

const value = (id: string, values: string[]): PagePropertyValue => ({
  id,
  workspaceId: "engineering",
  pageId: "page-1",
  propertyDefinitionId: "status",
  values,
});

describe("property model", () => {
  it("builds a compact summary from assigned active properties only", () => {
    const definitions = [
      definition,
      { ...definition, id: "owner", name: "Owner", valueKind: "text" as const },
      { ...definition, id: "retired", name: "Retired", state: "retired" as const },
    ];
    const values = [
      value("v1", ["Approved"]),
      { ...value("v2", ["Ada"]), propertyDefinitionId: "owner" },
      { ...value("v3", ["Hidden"]), propertyDefinitionId: "retired" },
    ];

    expect(getPropertySummary(definitions, values)).toEqual([
      { id: "status", name: "Status", value: "Approved", valueKind: "select" },
      { id: "owner", name: "Owner", value: "Ada", valueKind: "text" },
    ]);
  });

  it("formats scalar and multi-value properties for the compact row", () => {
    expect(formatPropertyValue({ ...definition, valueKind: "text" }, value("v1", ["Roadmap"]))).toBe("Roadmap");
    expect(formatPropertyValue({ ...definition, valueKind: "tags" }, value("v2", ["one", "two"]))).toBe("one, two");
    expect(formatPropertyValue({ ...definition, valueKind: "relation" }, value("v3", ["page-a", "page-b"]))).toBe("page-a, page-b");
  });

  it("upserts a page value without mutating the previous collection", () => {
    const previous = [value("v1", ["Draft"] )];
    const next = upsertPagePropertyValue(previous, value("v1", ["Approved"]));
    expect(next).toEqual([value("v1", ["Approved"])]);
    expect(previous).toEqual([value("v1", ["Draft"])]);
  });

  it("removes only the selected page value", () => {
    const previous = [value("v1", ["Draft"]), value("v2", ["Approved"] )];
    expect(removePagePropertyValue(previous, "v1")).toEqual([value("v2", ["Approved"])]);
  });

  it("assigns a stable palette color to the same option text", () => {
    expect(colorForTagValue("Draft")).toBe(colorForTagValue("Draft"));
    expect(tagPaletteNames).toContain(colorForTagValue("Draft"));
  });

  it("spreads distinct option text across more than one palette color", () => {
    const colors = new Set(["Draft", "Approved", "Blocked", "Done", "Archived"].map(colorForTagValue));
    expect(colors.size).toBeGreaterThan(1);
  });
});
