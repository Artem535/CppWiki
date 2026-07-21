// OpenAPI/Swagger spec document kind renderer (issue #107, ADR-017): a read-only viewer over the
// document's raw OpenAPI JSON, rendered via swagger-ui-react (bundled offline through Vite — no
// CDN script tag, unlike the server's own swagger_ui_handler, since this app must keep working
// fully offline). JSON only for v1, no YAML support. There is no "edit this spec through a form"
// affordance and never will be per the agreed v1 scope — content changes only via the raw-source
// import/export path (see page_helpers.cc's FileDialogNameFilterForKind/DetectImportableDocumentKind).
import { useEffect, useRef, useState } from "react";
import SwaggerUI from "swagger-ui-react";
import "swagger-ui-react/swagger-ui.css";

import type { EditorBridge } from "../bridge/editorBridge";

function parseOpenApiSpecJson(rawContent: string): object | null {
  const trimmed = rawContent.trim();
  if (trimmed.length === 0) {
    return {};
  }
  try {
    const parsed: unknown = JSON.parse(trimmed);
    if (parsed !== null && typeof parsed === "object") {
      return parsed as object;
    }
    return null;
  } catch {
    return null;
  }
}

export function OpenApiSpecView({
  bridge: _bridge,
  pageId,
  editable: _editable,
  rawContent,
}: {
  bridge: EditorBridge | null;
  pageId: string;
  editable: boolean;
  rawContent: string | undefined;
}) {
  const [spec, setSpec] = useState<object | null>(() => parseOpenApiSpecJson(rawContent ?? ""));
  const [parseFailed, setParseFailed] = useState(false);
  const loaded_page_id = useRef<string | null>(null);

  // Re-parse whenever a different document (or the same one reloaded) is handed to us — mirrors
  // NotebookView.tsx's/ProjectBoardView's reset-on-pageId-change pattern.
  useEffect(() => {
    if (loaded_page_id.current === pageId) {
      return;
    }
    loaded_page_id.current = pageId;
    const parsed = parseOpenApiSpecJson(rawContent ?? "");
    setSpec(parsed);
    setParseFailed(parsed === null);
  }, [pageId, rawContent]);

  if (parseFailed) {
    return (
      <div className="empty-state" data-testid="openapi-spec-parse-error">
        <h1>Could not read OpenAPI spec</h1>
        <p>The stored document is not valid JSON.</p>
      </div>
    );
  }

  return (
    <div className="openapi-spec-view" data-testid="openapi-spec-view">
      <SwaggerUI
        spec={spec ?? {}}
        // Read-only viewer (v1 scope): "Try it out" would let this embedded JS context issue raw
        // HTTP requests directly, bypassing the EditorBridge trust boundary (bridge/editor_bridge.cc)
        // that every other network/file/token access in this app crosses through. Disabled, not
        // just hidden, by leaving no submit methods enabled.
        tryItOutEnabled={false}
        supportedSubmitMethods={[]}
      />
    </div>
  );
}
