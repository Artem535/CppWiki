import { describe, expect, it } from "vitest";
import {
  formatPropertyValue,
  removePagePropertyValue,
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
});
