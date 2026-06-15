// Pure, component-free filtering logic for the Log page, separate from the
// LogLines component.

export type Level = "E" | "W" | "I" | "D" | "V";

/** Selectable levels, most to least severe, for the min-level control. */
export const LEVELS: { value: Level; label: string }[] = [
  { value: "E", label: "Error" },
  { value: "W", label: "Warning" },
  { value: "I", label: "Info" },
  { value: "D", label: "Debug" },
  { value: "V", label: "Verbose" },
];

const RANK: Record<Level, number> = { E: 0, W: 1, I: 2, D: 3, V: 4 };

const LEVEL_CLASS: Record<Level, string> = {
  E: "text-danger",
  W: "text-warning",
  I: "text-body",
  D: "text-secondary",
  V: "text-secondary",
};

// Effective level for a line before any explicit level has been seen, e.g. a
// leading continuation line.
const DEFAULT_LEVEL: Level = "I";

// The web UI polls these endpoints (the Log page hits /api/log, the status page
// hits /api/info) and the HTTP server logs every request, so the viewer captures
// its own polling. The http_server tag is part of the marker so a user line that
// merely mentions the path is not mistaken for a poll.
const SELF_POLL_MARKERS = [
  "http_server.cpp: Handling request: /api/log",
  "http_server.cpp: Handling request: /api/info",
];

/** The esp-idf level letter of a line, or null for a continuation line. */
function lineLevel(text: string): Level | null {
  const c = text.charAt(0);
  return c === "E" || c === "W" || c === "I" || c === "D" || c === "V"
    ? c
    : null;
}

/** Bootstrap text class for an effective level. */
export function levelClass(level: Level): string {
  return LEVEL_CLASS[level];
}

export function isSelfPolling(text: string): boolean {
  return SELF_POLL_MARKERS.some((marker) => text.includes(marker));
}

export interface LogFilter {
  minLevel: Level;
  query: string;
}

export interface LeveledLine<T> {
  line: T;
  level: Level;
}

/**
 * Return the lines that pass the filter, each tagged with its effective level.
 *
 * Effective level is carried forward across continuation lines (those without a
 * level prefix) so multi-line entries inherit their parent's level. The carry
 * runs over every line, including filtered-out ones, so inheritance is stable
 * regardless of what the filter hides. A line passes when its effective level is
 * at least as severe as the minimum AND the query (case-insensitive substring)
 * matches. Self-poll suppression happens at ingest, not here.
 */
export function visibleLines<T extends { text: string }>(
  lines: T[],
  filter: LogFilter,
): LeveledLine<T>[] {
  const maxRank = RANK[filter.minLevel];
  const query = filter.query.trim().toLowerCase();
  const result: LeveledLine<T>[] = [];
  let effective: Level = DEFAULT_LEVEL;

  for (const line of lines) {
    const explicit = lineLevel(line.text);
    if (explicit !== null) {
      effective = explicit;
    }
    if (RANK[effective] > maxRank) {
      continue;
    }
    if (query !== "" && !line.text.toLowerCase().includes(query)) {
      continue;
    }
    result.push({ line, level: effective });
  }

  return result;
}
