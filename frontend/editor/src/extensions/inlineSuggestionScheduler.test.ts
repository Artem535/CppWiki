import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { InlineSuggestionScheduler } from "./inlineSuggestionScheduler";

describe("InlineSuggestionScheduler", () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("does not fetch before the debounce elapses", () => {
    const fetchCompletion = vi.fn();
    const onSuggestion = vi.fn();
    const scheduler = new InlineSuggestionScheduler({ fetchCompletion, onSuggestion }, 450);

    scheduler.schedule("Hello, wor");
    vi.advanceTimersByTime(449);

    expect(fetchCompletion).not.toHaveBeenCalled();
  });

  it("fetches once the debounce elapses", async () => {
    const fetchCompletion = vi.fn().mockResolvedValue("ld!");
    const onSuggestion = vi.fn();
    const scheduler = new InlineSuggestionScheduler({ fetchCompletion, onSuggestion }, 450);

    scheduler.schedule("Hello, wor");
    await vi.advanceTimersByTimeAsync(450);

    expect(fetchCompletion).toHaveBeenCalledTimes(1);
    expect(fetchCompletion).toHaveBeenCalledWith("Hello, wor", expect.any(AbortSignal));
    expect(onSuggestion).toHaveBeenCalledWith("ld!", "Hello, wor");
  });

  it("restarts the debounce on every schedule() call (typing keeps resetting the timer)", async () => {
    const fetchCompletion = vi.fn().mockResolvedValue("!");
    const onSuggestion = vi.fn();
    const scheduler = new InlineSuggestionScheduler({ fetchCompletion, onSuggestion }, 450);

    scheduler.schedule("Hello");
    vi.advanceTimersByTime(300);
    scheduler.schedule("Hello,");
    vi.advanceTimersByTime(300);
    scheduler.schedule("Hello, world");
    await vi.advanceTimersByTimeAsync(450);

    expect(fetchCompletion).toHaveBeenCalledTimes(1);
    expect(fetchCompletion).toHaveBeenCalledWith("Hello, world", expect.any(AbortSignal));
  });

  it("does not schedule anything for blank context text", () => {
    const fetchCompletion = vi.fn();
    const onSuggestion = vi.fn();
    const scheduler = new InlineSuggestionScheduler({ fetchCompletion, onSuggestion }, 450);

    scheduler.schedule("   ");
    vi.advanceTimersByTime(10_000);

    expect(fetchCompletion).not.toHaveBeenCalled();
  });

  it("aborts the in-flight request's signal when cancel() is called before it resolves", async () => {
    let capturedSignal: AbortSignal | undefined;
    const fetchCompletion = vi.fn(
      (_context: string, signal: AbortSignal) =>
        new Promise<string>(() => {
          capturedSignal = signal;
          // Never resolves within the test; we only care about the signal.
        }),
    );
    const onSuggestion = vi.fn();
    const scheduler = new InlineSuggestionScheduler({ fetchCompletion, onSuggestion }, 450);

    scheduler.schedule("Hello, wor");
    await vi.advanceTimersByTimeAsync(450);

    expect(capturedSignal?.aborted).toBe(false);

    scheduler.cancel();

    expect(capturedSignal?.aborted).toBe(true);
  });

  it("ignores a late-resolving fetchCompletion result if cancel() ran first (stale response guard)", async () => {
    let resolveFirst: ((value: string) => void) | undefined;
    const fetchCompletion = vi
      .fn()
      .mockImplementationOnce(
        () =>
          new Promise<string>((resolve) => {
            resolveFirst = resolve;
          }),
      )
      .mockResolvedValueOnce("second");
    const onSuggestion = vi.fn();
    const scheduler = new InlineSuggestionScheduler({ fetchCompletion, onSuggestion }, 450);

    scheduler.schedule("first context");
    await vi.advanceTimersByTimeAsync(450);
    expect(fetchCompletion).toHaveBeenCalledTimes(1);

    // User kept typing before the first request resolved: cancel, then a
    // second request fires and resolves quickly.
    scheduler.schedule("second context");
    await vi.advanceTimersByTimeAsync(450);
    expect(fetchCompletion).toHaveBeenCalledTimes(2);
    expect(onSuggestion).toHaveBeenCalledTimes(1);
    expect(onSuggestion).toHaveBeenCalledWith("second", "second context");

    // The stale first request finally resolves; it must not clobber the
    // already-accepted second suggestion.
    resolveFirst?.("stale, should be ignored");
    await Promise.resolve();
    await Promise.resolve();
    expect(onSuggestion).toHaveBeenCalledTimes(1);
  });

  it("does not surface a suggestion when fetchCompletion rejects", async () => {
    const fetchCompletion = vi.fn().mockRejectedValue(new Error("provider timeout"));
    const onSuggestion = vi.fn();
    const scheduler = new InlineSuggestionScheduler({ fetchCompletion, onSuggestion }, 450);

    scheduler.schedule("Hello, wor");
    await vi.advanceTimersByTimeAsync(450);
    await Promise.resolve();

    expect(onSuggestion).not.toHaveBeenCalled();
  });

  it("does not surface a blank/whitespace-only completion", async () => {
    const fetchCompletion = vi.fn().mockResolvedValue("   \n  ");
    const onSuggestion = vi.fn();
    const scheduler = new InlineSuggestionScheduler({ fetchCompletion, onSuggestion }, 450);

    scheduler.schedule("Hello, wor");
    await vi.advanceTimersByTimeAsync(450);

    expect(onSuggestion).not.toHaveBeenCalled();
  });
});
