import type { EditorBridge } from "./editorBridge";

/**
 * Runs a single inline-completion request (issue #59) through the same
 * ADR-012 bridge transport used by `BridgeChatTransport`, but simplified: no
 * `ai` SDK `UIMessage`/stream-chunk plumbing, just "send this context text,
 * get the concatenated completion back (or an AbortError if cancelled)".
 *
 * `signal` cancels client-side only: once `startAiRequest` has handed the
 * request to C++, the outbound HTTP call to the provider is not aborted
 * server-side (there is no bridge method to cancel an in-flight request as
 * of this writing). Aborting here just means the eventual chunks/completion/
 * failure are ignored, which is what "cancel any in-flight request if the
 * user keeps typing" requires from the UI's perspective (no stale ghost text
 * ever appears), even though the provider call itself runs to completion.
 */
export function runInlineCompletion(
  bridge: EditorBridge,
  contextText: string,
  signal: AbortSignal,
): Promise<string> {
  return new Promise((resolve, reject) => {
    if (signal.aborted) {
      reject(new DOMException("Aborted", "AbortError"));
      return;
    }

    let settled = false;
    let accumulatedText = "";
    let unsubscribeChunk: () => void = () => undefined;
    let unsubscribeCompleted: () => void = () => undefined;
    let unsubscribeFailed: () => void = () => undefined;

    const cleanup = () => {
      unsubscribeChunk();
      unsubscribeCompleted();
      unsubscribeFailed();
      signal.removeEventListener("abort", onAbort);
    };

    const onAbort = () => {
      if (settled) {
        return;
      }
      settled = true;
      cleanup();
      reject(new DOMException("Aborted", "AbortError"));
    };
    signal.addEventListener("abort", onAbort);

    // `prompt` carries the local context to continue; `context` is left
    // empty since, unlike rewrite/autocomplete, there is no separate
    // instruction/selection to prepend (see AiChatService::BuildRequestBody's
    // "inline" system prefix, which already tells the model what to do with
    // `prompt` alone).
    void bridge.startAiRequest(contextText, "", "inline").then((requestId) => {
      if (settled) {
        return;
      }

      unsubscribeChunk = bridge.onAiChunkReceived((chunkRequestId, chunk) => {
        if (chunkRequestId !== requestId) {
          return;
        }
        accumulatedText += chunk;
      });

      unsubscribeCompleted = bridge.onAiRequestCompleted((completedRequestId) => {
        if (completedRequestId !== requestId || settled) {
          return;
        }
        settled = true;
        cleanup();
        resolve(accumulatedText);
      });

      unsubscribeFailed = bridge.onAiRequestFailed((failedRequestId, error) => {
        if (failedRequestId !== requestId || settled) {
          return;
        }
        settled = true;
        cleanup();
        reject(new Error(error));
      });
    });
  });
}
