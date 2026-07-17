export const snapshotDebounceMs = 500;

// Inline ghost-text suggestions (issue #59). Debounce is chosen from the
// issue's suggested 300-600ms range: long enough to avoid firing on every
// keystroke, short enough that a pause reads as "waiting for a suggestion"
// rather than a stall. contextChars caps how much preceding text is sent per
// request (local context only, not the whole document) to keep prompts small
// given the self-hosted provider's higher per-request latency.
export const inlineSuggestionDebounceMs = 450;
export const inlineSuggestionContextChars = 800;

export const mockPageId = "mock-page";
export const mockHeadingBlockId = "mock-heading";
export const mockBodyBlockId = "mock-body";
export const mockPageTitle = "Welcome to CppWiki";
export const mockHeadingText = "CppWiki";
export const mockBodyText = "Running without Qt bridge.";
export const mockPageCreatedAt = "2026-06-16T00:00:00.000Z";
export const mockPageUpdatedAt = "2026-06-16T00:00:00.000Z";

export const emptyStateTitle = "Select a page";
export const emptyStateMessage =
  "Choose a document from the page list. The editor will open it here and save changes automatically while you write.";
