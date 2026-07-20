import { describe, expect, it } from "vitest";

import { languageExtensionFor } from "./codeCellLanguage";

describe("languageExtensionFor", () => {
  it("resolves an extension for python (the notebook-level default)", () => {
    expect(languageExtensionFor("python")).not.toBeNull();
  });

  it("resolves an extension for javascript/typescript variants", () => {
    expect(languageExtensionFor("javascript")).not.toBeNull();
    expect(languageExtensionFor("typescript")).not.toBeNull();
  });

  it("resolves an extension for c/c++ variants", () => {
    expect(languageExtensionFor("c")).not.toBeNull();
    expect(languageExtensionFor("c++")).not.toBeNull();
    expect(languageExtensionFor("cpp")).not.toBeNull();
  });

  it("is case-insensitive", () => {
    expect(languageExtensionFor("Python")).not.toBeNull();
  });

  it("returns null for an unsupported language rather than mislabeling it", () => {
    expect(languageExtensionFor("r")).toBeNull();
    expect(languageExtensionFor("julia")).toBeNull();
  });
});
