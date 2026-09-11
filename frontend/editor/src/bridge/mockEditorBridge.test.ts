import { describe, expect, test } from "vitest";
import { isMockPropertiesPreview } from "./mockEditorBridge";

describe("mock properties preview", () => {
  test("only enables the properties fixture for the explicit preview query", () => {
    expect(isMockPropertiesPreview("?preview=properties")).toBe(true);
    expect(isMockPropertiesPreview("?preview=other")).toBe(false);
    expect(isMockPropertiesPreview("")).toBe(false);
  });
});
