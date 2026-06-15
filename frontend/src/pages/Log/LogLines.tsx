import { ToastMessage } from "components/ToastMessage";
import { APP_CONFIG } from "config";
import { type JSX } from "preact";
import { useCallback, useEffect, useRef, useState } from "preact/hooks";

interface LogResponse {
  session: number;
  next: number;
  gap: boolean;
  lines: string[];
}

interface LogLine {
  id: number;
  text: string;
}

type ConnState = "connecting" | "live" | "reconnecting";

const POLL_INTERVAL_MS = 500;
const FETCH_TIMEOUT_MS = 4000; // abort a stalled request so the loop recovers
const MAX_BACKOFF_MS = 10000; // ceiling on poll spacing while the device is down
const MAX_LINES = 1000;
const RECONNECT_THRESHOLD = 3; // consecutive failures (~1.5 s) before "reconnecting"
const BOTTOM_TOLERANCE_PX = 40;

/**
 * Bootstrap text class for a line, keyed on the esp-idf level letter. Returns
 * null for continuation lines (backtraces, multi-line messages) so they can
 * inherit the previous line's level.
 */
function levelClass(text: string): string | null {
  switch (text.charAt(0)) {
    case "E":
      return "text-danger";
    case "W":
      return "text-warning";
    case "I":
      return "text-body";
    case "D":
    case "V":
      return "text-secondary";
    default:
      return null;
  }
}

export function LogLines(): JSX.Element {
  const [lines, setLines] = useState<LogLine[]>([]);
  const [connState, setConnState] = useState<ConnState>("connecting");
  const [following, setFollowing] = useState(true);
  const [gapNotice, setGapNotice] = useState(false);
  const [restartNotice, setRestartNotice] = useState(false);

  const cursorRef = useRef<number | null>(null);
  const sessionRef = useRef<number | null>(null);
  const nextIdRef = useRef(0);
  const failuresRef = useRef(0);
  const followingRef = useRef(true);
  const containerRef = useRef<HTMLDivElement>(null);

  function setFollow(value: boolean): void {
    followingRef.current = value;
    setFollowing(value);
  }

  // Stable identities: ToastMessage/useToast lists onHide in an effect
  // dependency, so a fresh closure each render would re-show the toast on
  // every poll.
  const dismissRestart = useCallback(() => setRestartNotice(false), []);
  const dismissGap = useCallback(() => setGapNotice(false), []);

  useEffect(() => {
    let cancelled = false;

    async function poll(): Promise<void> {
      const url =
        cursorRef.current === null
          ? APP_CONFIG.log_path
          : `${APP_CONFIG.log_path}?since=${cursorRef.current}`;
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS);
      try {
        const response = await fetch(url, { signal: controller.signal });
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}`);
        }
        const data = (await response.json()) as LogResponse;
        if (cancelled) {
          return;
        }
        failuresRef.current = 0;
        setConnState("live");

        // Detect a device reboot: the per-boot session id changes, or the
        // sequence space reset below our cursor. Drop the stale buffer and
        // refetch the full buffer (no cursor) on the next poll.
        const reboot =
          sessionRef.current !== null &&
          (data.session !== sessionRef.current ||
            (cursorRef.current !== null && data.next < cursorRef.current));
        if (reboot) {
          sessionRef.current = data.session;
          cursorRef.current = null;
          setLines([]);
          setRestartNotice(true);
          return;
        }

        sessionRef.current = data.session;
        cursorRef.current = data.next;
        if (data.gap) {
          setGapNotice(true);
        }
        if (data.lines.length > 0) {
          setLines((prev) => {
            const added = data.lines.map((text) => ({
              id: nextIdRef.current++,
              text,
            }));
            const combined = prev.concat(added);
            return combined.length > MAX_LINES
              ? combined.slice(combined.length - MAX_LINES)
              : combined;
          });
        }
      } catch {
        if (cancelled) {
          return;
        }
        // Do not halt polling on error — keep retrying so a transient WiFi
        // blip does not kill the tail.
        failuresRef.current += 1;
        if (failuresRef.current >= RECONNECT_THRESHOLD) {
          setConnState("reconnecting");
        }
      } finally {
        clearTimeout(timeout);
      }
    }

    // Self-scheduling loop: the next poll is queued only after the current one
    // settles, so a slow device cannot accumulate overlapping in-flight
    // requests. Spacing backs off once failures pass the reconnect threshold.
    let timer: ReturnType<typeof setTimeout>;
    async function loop(): Promise<void> {
      await poll();
      if (cancelled) {
        return;
      }
      let delay = POLL_INTERVAL_MS;
      if (failuresRef.current >= RECONNECT_THRESHOLD) {
        const over = failuresRef.current - RECONNECT_THRESHOLD;
        delay = Math.min(POLL_INTERVAL_MS * 2 ** (over + 1), MAX_BACKOFF_MS);
      }
      timer = setTimeout(() => void loop(), delay);
    }

    void loop();
    return () => {
      cancelled = true;
      clearTimeout(timer);
    };
  }, []);

  // Follow the tail only while pinned to the bottom.
  useEffect(() => {
    const el = containerRef.current;
    if (el !== null && followingRef.current) {
      el.scrollTop = el.scrollHeight;
    }
  }, [lines]);

  function handleScroll(): void {
    const el = containerRef.current;
    if (el === null) {
      return;
    }
    const atBottom =
      el.scrollHeight - el.scrollTop - el.clientHeight < BOTTOM_TOLERANCE_PX;
    if (atBottom !== followingRef.current) {
      setFollow(atBottom);
    }
  }

  function jumpToLatest(): void {
    const el = containerRef.current;
    if (el !== null) {
      el.scrollTop = el.scrollHeight;
    }
    setFollow(true);
  }

  // Continuation lines inherit the previous line's level class.
  let lastClass = "text-body";
  const rendered = lines.map((line) => {
    const cls = levelClass(line.text);
    if (cls !== null) {
      lastClass = cls;
    }
    return (
      <div key={line.id} className={`text-nowrap ${lastClass}`}>
        {line.text}
      </div>
    );
  });

  let statusLabel: string;
  let statusClass: string;
  if (connState === "live") {
    statusLabel = lines.length === 0 ? "Waiting for output…" : "Live";
    statusClass = "text-bg-success";
  } else if (connState === "reconnecting") {
    statusLabel = "Reconnecting…";
    statusClass = "text-bg-warning";
  } else {
    statusLabel = "Connecting…";
    statusClass = "text-bg-secondary";
  }

  return (
    <div>
      <div className="d-flex align-items-center gap-2 mb-2">
        <span className={`badge rounded-pill px-3 py-2 ${statusClass}`}>
          {statusLabel}
        </span>
        {!following && (
          <button
            type="button"
            className="btn btn-sm btn-primary rounded-pill"
            onClick={jumpToLatest}
          >
            Jump to latest ↓
          </button>
        )}
      </div>

      <div
        ref={containerRef}
        onScroll={handleScroll}
        className="border rounded bg-body-tertiary p-2 font-monospace small overflow-auto"
        style="height: 70vh;"
        tabIndex={0}
        aria-label="Device log"
      >
        {rendered}
      </div>

      <ToastMessage
        color="text-bg-warning"
        show={restartNotice}
        onHide={dismissRestart}
      >
        Device restarted — reloading log.
      </ToastMessage>
      <ToastMessage color="text-bg-warning" show={gapNotice} onHide={dismissGap}>
        Some lines scrolled past (device keeps only ~2 s of log).
      </ToastMessage>
    </div>
  );
}
