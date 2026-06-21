import { backoffMs, createRequestQueue } from "./requestQueue";

/** Yield to the microtask/timer queue so scheduled work can progress. */
const tick = (): Promise<void> =>
  new Promise((resolve) => setTimeout(resolve, 0));

/** An error that the queue must treat as terminal (never retried). */
const abortError = (): DOMException =>
  new DOMException("The operation was aborted.", "AbortError");

describe("createRequestQueue", () => {
  it("never runs more than maxConcurrency tasks at once", async () => {
    const schedule = createRequestQueue(2, { baseDelayMs: 0, maxDelayMs: 0 });
    let active = 0;
    let maxActive = 0;
    const release: Array<() => void> = [];

    const task = (): Promise<void> =>
      new Promise<void>((resolve) => {
        active++;
        maxActive = Math.max(maxActive, active);
        release.push(() => {
          active--;
          resolve();
        });
      });

    const all = Promise.all(Array.from({ length: 6 }, () => schedule(task)));
    await tick();

    expect(active).toBe(2);

    // Drain: completing one in-flight task admits the next queued one.
    while (release.length > 0) {
      release.shift()!();
      await tick();
    }
    await all;

    expect(maxActive).toBe(2);
  });

  it("retries a rejecting task with backoff, then resolves", async () => {
    const schedule = createRequestQueue(2, { baseDelayMs: 0, maxDelayMs: 0 });
    let attempts = 0;

    const result = await schedule(() => {
      attempts++;
      if (attempts < 3) return Promise.reject(new Error("boom"));
      return Promise.resolve("ok");
    });

    expect(result).toBe("ok");
    expect(attempts).toBe(3);
  });

  it("gives up after maxRetries and rejects with the last error", async () => {
    const schedule = createRequestQueue(2, { baseDelayMs: 0, maxDelayMs: 0 });
    let attempts = 0;

    await expect(
      schedule(
        () => {
          attempts++;
          return Promise.reject(new Error("always"));
        },
        { maxRetries: 2 },
      ),
    ).rejects.toThrow("always");

    expect(attempts).toBe(3); // 1 initial + 2 retries
  });

  it("does not retry when shouldRetry returns false", async () => {
    const schedule = createRequestQueue(2, { baseDelayMs: 0, maxDelayMs: 0 });
    let attempts = 0;

    await expect(
      schedule(
        () => {
          attempts++;
          return Promise.reject(new Error("404"));
        },
        { shouldRetry: () => false },
      ),
    ).rejects.toThrow("404");

    expect(attempts).toBe(1);
  });

  it("never retries an AbortError, even if shouldRetry would allow it", async () => {
    const schedule = createRequestQueue(2, { baseDelayMs: 0, maxDelayMs: 0 });
    let attempts = 0;

    await expect(
      schedule(
        () => {
          attempts++;
          return Promise.reject(abortError());
        },
        { shouldRetry: () => true },
      ),
    ).rejects.toMatchObject({ name: "AbortError" });

    expect(attempts).toBe(1);
  });

  it("aborting a queued task dequeues it without leaking a slot", async () => {
    const schedule = createRequestQueue(1, { baseDelayMs: 0, maxDelayMs: 0 });

    let releaseFirst!: () => void;
    const first = schedule(
      () => new Promise<void>((resolve) => (releaseFirst = resolve)),
    );
    await tick(); // first occupies the only slot

    const ac = new AbortController();
    const second = schedule(() => Promise.resolve("second"), {
      signal: ac.signal,
    });
    await tick(); // second is queued behind first

    ac.abort();
    await expect(second).rejects.toMatchObject({ name: "AbortError" });

    // If the aborted waiter leaked the slot, the third task would never run.
    releaseFirst();
    await first;
    await expect(schedule(() => Promise.resolve("third"))).resolves.toBe(
      "third",
    );
  });

  it("aborting an in-flight task rejects it and frees the slot", async () => {
    const schedule = createRequestQueue(1, { baseDelayMs: 0, maxDelayMs: 0 });

    const ac = new AbortController();
    const inFlight = schedule(
      (signal) =>
        new Promise<void>((_, reject) => {
          signal?.addEventListener("abort", () => reject(abortError()));
        }),
      { signal: ac.signal },
    );
    await tick();

    ac.abort();
    await expect(inFlight).rejects.toMatchObject({ name: "AbortError" });

    await expect(schedule(() => Promise.resolve("next"))).resolves.toBe("next");
  });

  it("aborting during the backoff between retries rejects and frees the slot", async () => {
    jest.spyOn(Math, "random").mockReturnValue(0.5); // 0.5 * 1000 = 500ms backoff
    const schedule = createRequestQueue(1, {
      baseDelayMs: 1000,
      maxDelayMs: 1000,
    });

    const ac = new AbortController();
    let attempts = 0;
    const failing = schedule(
      () => {
        attempts++;
        return Promise.reject(new Error("transient"));
      },
      { signal: ac.signal },
    );
    await tick(); // first attempt failed; now sleeping in the backoff delay
    expect(attempts).toBe(1);

    ac.abort();
    await expect(failing).rejects.toMatchObject({ name: "AbortError" });
    expect(attempts).toBe(1); // never retried after abort

    // The slot was released despite aborting mid-backoff.
    await expect(schedule(() => Promise.resolve("next"))).resolves.toBe("next");
    jest.restoreAllMocks();
  });

  it("rejects immediately if the signal is already aborted", async () => {
    const schedule = createRequestQueue(2, { baseDelayMs: 0, maxDelayMs: 0 });
    let ran = false;

    const ac = new AbortController();
    ac.abort();

    await expect(
      schedule(
        () => {
          ran = true;
          return Promise.resolve("x");
        },
        { signal: ac.signal },
      ),
    ).rejects.toMatchObject({ name: "AbortError" });

    expect(ran).toBe(false);
  });
});

describe("backoffMs", () => {
  afterEach(() => jest.restoreAllMocks());

  it("grows exponentially per attempt and is capped at maxMs", () => {
    jest.spyOn(Math, "random").mockReturnValue(1); // sample the full ceiling
    expect(backoffMs(1, 100, 10000)).toBeCloseTo(100); // 100 * 2^0
    expect(backoffMs(2, 100, 10000)).toBeCloseTo(200); // 100 * 2^1
    expect(backoffMs(3, 100, 10000)).toBeCloseTo(400); // 100 * 2^2
    expect(backoffMs(10, 100, 1000)).toBeCloseTo(1000); // capped at maxMs
  });

  it("applies full jitter within [0, ceiling]", () => {
    const random = jest.spyOn(Math, "random");
    random.mockReturnValue(0.5);
    expect(backoffMs(3, 100, 10000)).toBeCloseTo(200); // 0.5 * 400
    random.mockReturnValue(0);
    expect(backoffMs(3, 100, 10000)).toBe(0);
  });
});
