import { ToastMessage } from "components/ToastMessage";
import { APP_CONFIG } from "config";
import { type JSX } from "preact";
import { useCallback, useEffect, useId, useRef, useState } from "preact/hooks";

import {
  isSelfPolling,
  LEVELS,
  levelClass,
  visibleLines,
  type Level,
} from "./filtering";

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

export function LogLines(): JSX.Element {
  const [lines, setLines] = useState<LogLine[]>([]);
  const [connState, setConnState] = useState<ConnState>("connecting");
  const [following, setFollowing] = useState(true);
  const [gapNotice, setGapNotice] = useState(false);
  const [restartNotice, setRestartNotice] = useState(false);

  // Display-only filters over the buffered tail. Default to Info+ and hide the
  // viewer's own /api/log polling lines.
  const [minLevel, setMinLevel] = useState<Level>("I");
  const [query, setQuery] = useState("");
  const [showPolling, setShowPolling] = useState(false);

  const cursorRef = useRef<number | null>(null);
  const sessionRef = useRef<number | null>(null);
  const nextIdRef = useRef(0);
  const failuresRef = useRef(0);
  const followingRef = useRef(true);
  const showPollingRef = useRef(false);
  const containerRef = useRef<HTMLDivElement>(null);
  const showPollingId = useId();

  function setFollow(value: boolean): void {
    followingRef.current = value;
    setFollowing(value);
  }

  // The poll loop reads showPolling via a ref so the long-lived effect closure
  // sees the current value without re-subscribing.
  function setShowPollingValue(value: boolean): void {
    showPollingRef.current = value;
    setShowPolling(value);
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
        // Drop the viewer's own polling lines at ingest unless the user opted
        // in, so self-poll noise never consumes the bounded line buffer and
        // evicts lines the user is waiting for.
        const incoming = showPollingRef.current
          ? data.lines
          : data.lines.filter((text) => !isSelfPolling(text));
        if (incoming.length > 0) {
          setLines((prev) => {
            const added = incoming.map((text) => ({
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

  // Follow the tail only while pinned to the bottom. Also re-anchor when a
  // filter change shrinks or grows the visible set.
  useEffect(() => {
    const el = containerRef.current;
    if (el !== null && followingRef.current) {
      el.scrollTop = el.scrollHeight;
    }
  }, [lines, minLevel, query]);

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

  const visible = visibleLines(lines, { minLevel, query });
  const rendered = visible.map(({ line, level }) => (
    <div key={line.id} className={`text-nowrap ${levelClass(level)}`}>
      {line.text}
    </div>
  ));

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

      <div className="d-flex align-items-center flex-wrap gap-2 mb-2">
        <select
          className="form-select form-select-sm w-auto"
          value={minLevel}
          onChange={(e) => setMinLevel(e.currentTarget.value as Level)}
          aria-label="Minimum log level (this level and more severe)"
        >
          {LEVELS.map((l) => (
            <option value={l.value} key={l.value}>
              {l.label}
            </option>
          ))}
        </select>
        <input
          type="search"
          className="form-control form-control-sm w-auto"
          placeholder="Filter text…"
          value={query}
          onInput={(e) => setQuery(e.currentTarget.value)}
          aria-label="Filter log text"
        />
        <div className="form-check form-switch ms-auto">
          <input
            className="form-check-input"
            type="checkbox"
            role="switch"
            id={showPollingId}
            checked={showPolling}
            onChange={(e) => setShowPollingValue(e.currentTarget.checked)}
          />
          <label className="form-check-label" htmlFor={showPollingId}>
            Show polling requests
          </label>
        </div>
      </div>

      <div
        ref={containerRef}
        onScroll={handleScroll}
        className="border rounded bg-body-tertiary p-2 font-monospace small overflow-auto"
        style="height: 70vh;"
        tabIndex={0}
        aria-label="Device log"
      >
        {rendered.length > 0 ? (
          rendered
        ) : (
          <div className="text-secondary fst-italic">No matching lines.</div>
        )}
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
