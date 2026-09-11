import {
  bridgeApiVersion,
  type DocumentSummary,
  type BridgeInfo,
  type BridgeResult,
  type EditorBridge,
  type InitialDocumentSnapshot,
  type LoadedDocument,
  type PropertyDefinition,
  type PagePropertyValue,
} from "./editorBridge";
import {
  mockBodyBlockId,
  mockBodyText,
  mockHeadingBlockId,
  mockHeadingText,
  mockPageCreatedAt,
  mockPageId,
  mockPageTitle,
  mockPageUpdatedAt,
} from "../constants";

const initialDocument: InitialDocumentSnapshot = [
  {
    id: mockHeadingBlockId,
    type: "heading",
    props: { level: 1 },
    content: [{ type: "text", text: mockHeadingText, styles: {} }],
    children: [],
  },
  {
    id: mockBodyBlockId,
    type: "paragraph",
    content: [{ type: "text", text: mockBodyText, styles: {} }],
    children: [],
  },
];

const documents: DocumentSummary[] = [
  {
    id: mockPageId,
    title: mockPageTitle,
    parentId: null,
    sortOrder: 0,
    createdAt: mockPageCreatedAt,
    updatedAt: mockPageUpdatedAt,
    kind: "wikiPage",
  },
];

const mockPropertyDefinitions: PropertyDefinition[] = [
  {
    id: "status",
    workspaceId: "default",
    name: "Status",
    valueKind: "select",
    options: ["Draft", "Approved"],
    state: "active",
  },
  {
    id: "owner",
    workspaceId: "default",
    name: "Owner",
    valueKind: "text",
    options: [],
    state: "active",
  },
  {
    id: "type",
    workspaceId: "default",
    name: "Type",
    valueKind: "select",
    options: ["Reference", "Project", "Note"],
    state: "active",
  },
  {
    id: "tags",
    workspaceId: "default",
    name: "Tags",
    valueKind: "tags",
    options: [],
    state: "active",
  },
  {
    id: "confidence",
    workspaceId: "default",
    name: "Confidence",
    valueKind: "number",
    options: [],
    state: "active",
  },
  {
    id: "review",
    workspaceId: "default",
    name: "Review date",
    valueKind: "date",
    options: [],
    state: "active",
  },
];
const mockPagePropertyValues: PagePropertyValue[] = [
  { id: "value-status", workspaceId: "default", pageId: mockPageId, propertyDefinitionId: "status", values: ["Draft"] },
  { id: "value-owner", workspaceId: "default", pageId: mockPageId, propertyDefinitionId: "owner", values: ["Alex"] },
  { id: "value-type", workspaceId: "default", pageId: mockPageId, propertyDefinitionId: "type", values: ["Reference"] },
  { id: "value-tags", workspaceId: "default", pageId: mockPageId, propertyDefinitionId: "tags", values: ["knowledge", "design"] },
  { id: "value-confidence", workspaceId: "default", pageId: mockPageId, propertyDefinitionId: "confidence", values: ["92"] },
  { id: "value-review", workspaceId: "default", pageId: mockPageId, propertyDefinitionId: "review", values: ["2026-09-18"] },
];

export function isMockPropertiesPreview(search: string): boolean {
  return new URLSearchParams(search).get("preview") === "properties";
}

function makeMockLoadedDocument(pageId: string): LoadedDocument {
  return {
    id: pageId,
    workspaceId: "default",
    title: mockPageTitle,
    parentId: null,
    sortOrder: 0,
    createdAt: mockPageCreatedAt,
    updatedAt: mockPageUpdatedAt,
    blocks: initialDocument,
    editable: true,
    lockOwner: null,
    accessMessage: "Document: local-only editing",
    kind: "wikiPage",
  };
}

const mockAiChunkListeners = new Set<(requestId: string, chunk: string) => void>();
const mockAiToolCallListeners = new Set<
  (requestId: string, toolName: string, argumentsJson: string) => void
>();
const mockAiCompletedListeners = new Set<(requestId: string) => void>();
const mockAiFailedListeners = new Set<(requestId: string, error: string) => void>();

export function createMockEditorBridge(): EditorBridge {
  const propertiesPreview =
    typeof window !== "undefined" && isMockPropertiesPreview(window.location.search);

  return {
    async getBridgeInfo(): Promise<BridgeResult<BridgeInfo>> {
      return {
        apiVersion: bridgeApiVersion,
        ok: true,
        result: {
          apiVersion: bridgeApiVersion,
          namespace: "wiki.documents",
          // Mock bridge (no Qt/QWebChannel): enable inline suggestions by
          // default so the feature is exercisable in `npm run dev` without a
          // full desktop build.
          aiInlineSuggestionsEnabled: true,
          methods: [
            "getBridgeInfo",
            "getInitialDocument",
            "listDocuments",
            "loadDocument",
            "openDocument",
            "updateSnapshot",
            "exportTextToFile",
            "importTextFromFile",
          ],
        },
      };
    },

    async getInitialDocument(): Promise<BridgeResult<InitialDocumentSnapshot>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: initialDocument };
    },

    async listDocuments(): Promise<BridgeResult<DocumentSummary[]>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: documents };
    },

    async loadDocument() {
      return {
        apiVersion: bridgeApiVersion,
        ok: true,
        result: makeMockLoadedDocument(mockPageId),
      };
    },

    async openDocument(pageId) {
      return {
        apiVersion: bridgeApiVersion,
        ok: true,
        result: makeMockLoadedDocument(pageId),
      };
    },

    async listPropertyDefinitions(workspaceId) {
      return { apiVersion: bridgeApiVersion, ok: true, result: mockPropertyDefinitions.filter((item) => item.workspaceId === workspaceId) };
    },

    async savePropertyDefinition(definition) {
      const next: PropertyDefinition = {
        id: definition.id ?? `property-${Date.now()}`,
        workspaceId: definition.workspaceId ?? "default",
        name: definition.name,
        groupName: definition.groupName,
        valueKind: definition.valueKind,
        options: definition.options ?? [],
        state: definition.state ?? "active",
      };
      const index = mockPropertyDefinitions.findIndex((item) => item.id === next.id);
      if (index < 0) mockPropertyDefinitions.push(next);
      else mockPropertyDefinitions[index] = next;
      return { apiVersion: bridgeApiVersion, ok: true, result: next };
    },

    async retirePropertyDefinition(definitionId) {
      const item = mockPropertyDefinitions.find((definition) => definition.id === definitionId);
      if (!item) return { apiVersion: bridgeApiVersion, ok: false, error: { code: "not_found", message: "Property definition was not found." } };
      item.state = "retired";
      return { apiVersion: bridgeApiVersion, ok: true, result: item };
    },

    async listPagePropertyValues(workspaceId, pageId) {
      return { apiVersion: bridgeApiVersion, ok: true, result: mockPagePropertyValues.filter((item) => item.workspaceId === workspaceId && item.pageId === pageId) };
    },

    async savePagePropertyValue(value) {
      const next: PagePropertyValue = { ...value, id: value.id ?? `value-${Date.now()}` };
      const index = mockPagePropertyValues.findIndex((item) => item.id === next.id);
      if (index < 0) mockPagePropertyValues.push(next);
      else mockPagePropertyValues[index] = next;
      return { apiVersion: bridgeApiVersion, ok: true, result: next };
    },

    async deletePagePropertyValue(valueId) {
      const index = mockPagePropertyValues.findIndex((item) => item.id === valueId);
      if (index >= 0) mockPagePropertyValues.splice(index, 1);
      return { apiVersion: bridgeApiVersion, ok: true, result: undefined };
    },

    async updateSnapshot(_pageId, _snapshot): Promise<BridgeResult<void>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: undefined };
    },

    // No native file dialog outside the Qt embedding (`npm run dev`) — mimic "user cancelled"
    // rather than pretending a file was chosen, since there's nowhere for content to go.
    async exportTextToFile(suggestedFileName, _nameFilter, _content) {
      return {
        apiVersion: bridgeApiVersion,
        ok: false,
        error: {
          code: "cancelled",
          message: `Export to disk is only available in the desktop app (would have saved "${suggestedFileName}").`,
        },
      };
    },

    async importTextFromFile(_nameFilter) {
      return {
        apiVersion: bridgeApiVersion,
        ok: false,
        error: { code: "cancelled", message: "Import from disk is only available in the desktop app." },
      };
    },

    async beginAttachmentUpload() {
      return {
        apiVersion: bridgeApiVersion,
        ok: false,
        error: {
          code: "unsupported",
          message: "Attachments are only available in the desktop app.",
        },
      };
    },

    async appendAttachmentChunk() {
      return {
        apiVersion: bridgeApiVersion,
        ok: false,
        error: { code: "unsupported", message: "Attachments are only available in the desktop app." },
      };
    },

    async completeAttachmentUpload() {
      return {
        apiVersion: bridgeApiVersion,
        ok: false,
        error: { code: "unsupported", message: "Attachments are only available in the desktop app." },
      };
    },

    async cancelAttachmentUpload() {
      return {
        apiVersion: bridgeApiVersion,
        ok: false,
        error: { code: "unsupported", message: "Attachments are only available in the desktop app." },
      };
    },

    async saveAttachmentToFile() {
      return {
        apiVersion: bridgeApiVersion,
        ok: false,
        error: { code: "unsupported", message: "Attachments are only available in the desktop app." },
      };
    },

    async pasteClipboardAttachment() {
      return {
        apiVersion: bridgeApiVersion,
        ok: false,
        error: { code: "unsupported", message: "Clipboard attachments are only available in the desktop app." },
      };
    },

    onDocumentOpenRequested() {
      return () => undefined;
    },

    onDocumentLoaded(callback) {
      if (propertiesPreview) {
        const timer = window.setTimeout(() => callback(makeMockLoadedDocument(mockPageId)), 0);
        return () => window.clearTimeout(timer);
      }
      return () => undefined;
    },

    onDocumentAccessChanged() {
      return () => undefined;
    },

    onDocumentLoadFailed() {
      return () => undefined;
    },

    onDocumentSelectionCleared() {
      return () => undefined;
    },
    onExportCurrentDocumentRequested() {
      return () => undefined;
    },

    async startAiRequest(prompt, contextText, _mode, toolName, toolSchemaJson) {
      const requestId = `mock-ai-${Math.random().toString(36).slice(2)}`;
      window.setTimeout(() => {
        if (toolName && toolSchemaJson) {
          // Mock tool-call reply: an empty operations array is a valid,
          // schema-conforming structured response for most xl-ai tool
          // schemas, and is enough to exercise the tool-call relay path.
          mockAiToolCallListeners.forEach((listener) =>
            listener(requestId, toolName, JSON.stringify({ operations: [] })),
          );
          mockAiCompletedListeners.forEach((listener) => listener(requestId));
          return;
        }

        const mockReply = `[mock ${prompt}] ${contextText}`.trim();
        for (const char of mockReply) {
          mockAiChunkListeners.forEach((listener) => listener(requestId, char));
        }
        mockAiCompletedListeners.forEach((listener) => listener(requestId));
      }, 10);
      return requestId;
    },

    onAiChunkReceived(callback) {
      mockAiChunkListeners.add(callback);
      return () => {
        mockAiChunkListeners.delete(callback);
      };
    },

    onAiToolCallReceived(callback) {
      mockAiToolCallListeners.add(callback);
      return () => {
        mockAiToolCallListeners.delete(callback);
      };
    },

    onAiRequestCompleted(callback) {
      mockAiCompletedListeners.add(callback);
      return () => {
        mockAiCompletedListeners.delete(callback);
      };
    },

    onAiRequestFailed(callback) {
      mockAiFailedListeners.add(callback);
      return () => {
        mockAiFailedListeners.delete(callback);
      };
    },
  };
}
