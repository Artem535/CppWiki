/**
 * Debounce + cancellation state machine for inline ghost-text suggestions
 * (issue #59), deliberately kept independent of ProseMirror/Tiptap so it can
 * be unit-tested in isolation (see inlineSuggestionScheduler.test.ts) without
 * a real editor instance.
 *
 * Lifecycle per call to `schedule(contextText)`:
 *  1. Any previously pending debounce timer or in-flight request is
 *     cancelled (its result, if it later arrives, is ignored).
 *  2. A new debounce timer starts; if nothing else calls `schedule()` or
 *     `cancel()` within `debounceMs`, `fetchCompletion` is invoked.
 *  3. If the resulting promise resolves with non-blank text and nothing
 *     cancelled it in the meantime, `onSuggestion` fires with that text.
 *
 * `cancel()` (called by the caller on every doc-changing transaction) is
 * what implements "continuing to type dismisses/cancels" from the issue.
 */
export interface InlineSuggestionSchedulerCallbacks {
  fetchCompletion: (contextText: string, signal: AbortSignal) => Promise<string>;
  onSuggestion: (suggestion: string, forContextText: string) => void;
}

export class InlineSuggestionScheduler {
  private timer: ReturnType<typeof setTimeout> | null = null;
  private controller: AbortController | null = null;
  private requestSeq = 0;

  constructor(
    private readonly callbacks: InlineSuggestionSchedulerCallbacks,
    private readonly debounceMs: number,
  ) {}

  /**
   * Cancels any pending debounce timer and in-flight request without firing
   * any callback. Safe to call when nothing is pending.
   */
  cancel(): void {
    if (this.timer !== null) {
      clearTimeout(this.timer);
      this.timer = null;
    }
    if (this.controller !== null) {
      this.controller.abort();
      this.controller = null;
    }
    // Invalidates any in-flight fire() whose fetchCompletion promise has not
    // resolved yet, even if it didn't observe the abort signal itself.
    this.requestSeq += 1;
  }

  /**
   * Cancels anything pending (see `cancel()`) and schedules a fresh
   * completion request for `contextText`, debounced by `debounceMs`. A
   * blank/whitespace-only `contextText` cancels without scheduling anything
   * new (there's nothing to continue).
   */
  schedule(contextText: string): void {
    this.cancel();

    if (contextText.trim().length === 0) {
      return;
    }

    this.timer = setTimeout(() => {
      this.timer = null;
      void this.fire(contextText);
    }, this.debounceMs);
  }

  private async fire(contextText: string): Promise<void> {
    const controller = new AbortController();
    this.controller = controller;
    const seq = this.requestSeq;

    let suggestion: string;
    try {
      suggestion = await this.callbacks.fetchCompletion(contextText, controller.signal);
    } catch {
      // Best-effort: a failed/aborted completion just means no ghost text
      // appears. There is no user-facing error UI for this in v1 (unlike the
      // toolbar/slash-menu AI features, which do surface errors).
      return;
    }

    if (seq !== this.requestSeq || this.controller !== controller) {
      // Cancelled (or superseded by a later schedule()/cancel() call) while
      // fetchCompletion was in flight.
      return;
    }
    this.controller = null;

    if (suggestion.trim().length === 0) {
      return;
    }

    this.callbacks.onSuggestion(suggestion, contextText);
  }

  dispose(): void {
    this.cancel();
  }
}
