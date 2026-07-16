import type { ChatTransport, UIMessage, UIMessageChunk } from "ai";
import type { EditorBridge } from "./editorBridge";

/**
 * Custom `ChatTransport` for `@blocknote/xl-ai`'s `AIExtension`, replacing the
 * package's default `DefaultChatTransport` (ADR-012). The default transport
 * calls `fetch` directly from browser JS; that is not allowed here —
 * `QWebEngineView` must never get raw network access. This transport instead
 * forwards every request through `EditorBridge`/`QWebChannel` to C++, which
 * performs the actual HTTP call (server-mediated or local-key fallback, see
 * the ADR-012 addendum) and relays the response back chunk-by-chunk via
 * bridge signals.
 *
 * Implements the `ai` SDK's `ChatTransport` interface, kept intentionally
 * minimal for the MVP scope (ADR-010): rewrite/improve and autocomplete only,
 * plain text streaming, no tool-calling/reasoning parts.
 */
export class BridgeChatTransport implements ChatTransport<UIMessage> {
  // The bridge is created asynchronously (QWebChannel handshake) after the
  // BlockNote editor instance itself, so this takes a getter rather than the
  // bridge directly; the getter is only invoked when a request is actually
  // sent, by which point the bridge is expected to be ready.
  constructor(
    private readonly getBridge: () => EditorBridge | null,
    private readonly mode: "rewrite" | "autocomplete",
  ) {}

  /**
   * Sends the latest user message (plus a flattened text rendition of any
   * prior messages, used as "context") through the bridge, and returns a
   * ReadableStream of `UIMessageChunk`s built from the bridge's
   * `aiChunkReceived` signal. The stream ends when `aiRequestCompleted`
   * fires, or errors when `aiRequestFailed` fires.
   */
  async sendMessages(options: {
    messages: UIMessage[];
  }): Promise<ReadableStream<UIMessageChunk>> {
    const bridge = this.getBridge();
    if (!bridge) {
      throw new Error("Editor bridge is not ready yet.");
    }

    const messages = options.messages ?? [];
    const lastMessage = messages[messages.length - 1];
    const prompt = ExtractText(lastMessage);
    const contextText = messages
      .slice(0, -1)
      .map((message) => ExtractText(message))
      .filter((text) => text.length > 0)
      .join("\n\n");

    const requestId = await bridge.startAiRequest(prompt, contextText, this.mode);
    const textPartId = `${requestId}-text`;

    return new ReadableStream<UIMessageChunk>({
      start(controller) {
        controller.enqueue({ type: "text-start", id: textPartId });

        const unsubscribeChunk = bridge.onAiChunkReceived((chunkRequestId, chunk) => {
          if (chunkRequestId !== requestId) {
            return;
          }
          controller.enqueue({ type: "text-delta", id: textPartId, delta: chunk });
        });

        const unsubscribeCompleted = bridge.onAiRequestCompleted((completedRequestId) => {
          if (completedRequestId !== requestId) {
            return;
          }
          controller.enqueue({ type: "text-end", id: textPartId });
          cleanup();
          controller.close();
        });

        const unsubscribeFailed = bridge.onAiRequestFailed((failedRequestId, error) => {
          if (failedRequestId !== requestId) {
            return;
          }
          controller.enqueue({ type: "error", errorText: error });
          cleanup();
          controller.close();
        });

        function cleanup() {
          unsubscribeChunk();
          unsubscribeCompleted();
          unsubscribeFailed();
        }
      },
    });
  }

  /**
   * Stream resumption is not supported in the MVP scope: AI requests here
   * are short-lived rewrite/autocomplete calls, not long-running chat
   * sessions that need to survive a reload.
   */
  async reconnectToStream(): Promise<ReadableStream<UIMessageChunk> | null> {
    return null;
  }
}

function ExtractText(message: UIMessage | undefined): string {
  if (!message || !Array.isArray(message.parts)) {
    return "";
  }
  return message.parts
    .filter((part): part is Extract<typeof part, { type: "text" }> => part.type === "text")
    .map((part) => part.text)
    .join("");
}
