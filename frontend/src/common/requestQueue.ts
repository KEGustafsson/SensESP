/**
 * A global concurrency limiter with retry-and-backoff for outgoing requests.
 *
 * The embedded device serves the web UI from a tiny socket pool. When the
 * configuration page fans out one fetch per card, an unbounded burst exhausts
 * that pool and the device drops connections, leaving cards blank. Routing
 * every request through a shared limiter keeps the number of concurrent
 * connections within what the device can accept, and bounded retry recovers the
 * occasional dropped request.
 *
 * The limiter is created with a factory so each instance owns its own state;
 * the module exports one shared instance that all callers funnel through.
 */

/** Maximum requests the shared queue keeps in flight at once. */
export const MAX_CONCURRENCY = 2;

const DEFAULT_MAX_RETRIES = 3;
const DEFAULT_BASE_DELAY_MS = 300;
const DEFAULT_MAX_DELAY_MS = 3000;

export interface QueueConfig {
  maxRetries?: number;
  baseDelayMs?: number;
  maxDelayMs?: number;
}

export interface ScheduleOptions {
  signal?: AbortSignal;
  /** Overrides the queue's default retry count for this request. */
  maxRetries?: number;
  /** Decides whether a given error is worth retrying. Defaults to always. */
  shouldRetry?: (error: unknown) => boolean;
}

export type ScheduleFn = <T>(
  task: (signal?: AbortSignal) => Promise<T>,
  options?: ScheduleOptions,
) => Promise<T>;

export function isAbortError(error: unknown): boolean {
  return (error as { name?: string } | null)?.name === "AbortError";
}

function abortError(): DOMException {
  return new DOMException("The operation was aborted.", "AbortError");
}

/**
 * Resolve after `ms`, or reject with an AbortError if the signal fires first.
 * The timer is always cleared so a pending backoff never outlives its caller.
 */
function delay(ms: number, signal?: AbortSignal): Promise<void> {
  return new Promise<void>((resolve, reject) => {
    if (signal?.aborted) {
      reject(abortError());
      return;
    }
    const timer = setTimeout(() => {
      cleanup();
      resolve();
    }, ms);
    const onAbort = (): void => {
      clearTimeout(timer);
      cleanup();
      reject(abortError());
    };
    const cleanup = (): void => signal?.removeEventListener("abort", onAbort);
    signal?.addEventListener("abort", onAbort, { once: true });
  });
}

/**
 * Full-jitter exponential backoff: a random delay in [0, base * 2^n], capped.
 * Jitter de-synchronizes retries so a burst that failed together does not
 * retry together and re-saturate the device.
 */
export function backoffMs(
  attempt: number,
  baseMs: number,
  maxMs: number,
): number {
  const ceiling = Math.min(maxMs, baseMs * 2 ** (attempt - 1));
  return Math.random() * ceiling;
}

export function createRequestQueue(
  maxConcurrency: number,
  config: QueueConfig = {},
): ScheduleFn {
  const {
    maxRetries: defaultMaxRetries = DEFAULT_MAX_RETRIES,
    baseDelayMs = DEFAULT_BASE_DELAY_MS,
    maxDelayMs = DEFAULT_MAX_DELAY_MS,
  } = config;

  let inFlight = 0;
  const waiters: Array<() => void> = [];

  // A slot is held for the whole lifetime of one scheduled call (including its
  // backoff waits) and released exactly once, so the in-flight count can never
  // leak or be double-counted. acquire() either takes a free slot or parks a
  // waiter that a later release() hands its slot to directly.
  function acquire(signal?: AbortSignal): Promise<void> {
    if (inFlight < maxConcurrency) {
      inFlight++;
      return Promise.resolve();
    }
    return new Promise<void>((resolve, reject) => {
      const grant = (): void => {
        cleanup();
        resolve();
      };
      const onAbort = (): void => {
        const index = waiters.indexOf(grant);
        if (index !== -1) waiters.splice(index, 1);
        cleanup();
        reject(abortError());
      };
      const cleanup = (): void => signal?.removeEventListener("abort", onAbort);
      waiters.push(grant);
      signal?.addEventListener("abort", onAbort, { once: true });
    });
  }

  function release(): void {
    const next = waiters.shift();
    if (next) {
      next(); // slot transfers to the waiter; inFlight unchanged
    } else {
      inFlight--;
    }
  }

  async function runWithRetry<T>(
    task: (signal?: AbortSignal) => Promise<T>,
    maxRetries: number,
    shouldRetry: (error: unknown) => boolean,
    signal?: AbortSignal,
  ): Promise<T> {
    let attempt = 0;
    for (;;) {
      if (signal?.aborted) throw abortError();
      try {
        const result = await task(signal);
        // A task can resolve in the same tick the signal fires; treat a
        // post-resolution abort as a cancellation so the caller ignores it.
        if (signal?.aborted) throw abortError();
        return result;
      } catch (error) {
        if (isAbortError(error)) throw error;
        attempt++;
        if (attempt > maxRetries || !shouldRetry(error)) throw error;
        await delay(backoffMs(attempt, baseDelayMs, maxDelayMs), signal);
      }
    }
  }

  return async function schedule<T>(
    task: (signal?: AbortSignal) => Promise<T>,
    options: ScheduleOptions = {},
  ): Promise<T> {
    const {
      signal,
      maxRetries = defaultMaxRetries,
      shouldRetry = () => true,
    } = options;

    if (signal?.aborted) throw abortError();

    await acquire(signal); // throws AbortError (and holds no slot) if aborted while queued
    try {
      return await runWithRetry(task, maxRetries, shouldRetry, signal);
    } finally {
      release();
    }
  };
}

/** Shared queue every config request funnels through. */
export const scheduleRequest = createRequestQueue(MAX_CONCURRENCY);
