import { APP_CONFIG } from "config";
import { type JsonObject } from "./jsonTypes";
import { isAbortError, scheduleRequest } from "./requestQueue";

/**
 * Represents the configuration data.
 */
export interface ConfigData {
  /**
   * The configuration data.
   */
  config: JsonObject;
  /**
   * The schema of the configuration data.
   */
  schema: JsonObject;
  /**
   * The human-readable title of the configuration item.
   */
  title: string;
  /**
   * The description of the configuration data.
   */
  description: string;
  /**
   * Whether applying this configuration requires a device restart.
   */
  requires_restart: boolean;
}

/** An HTTP response error carrying the status code, so callers can decide
 * whether the failure is worth retrying. */
export class HttpError extends Error {
  constructor(
    public readonly status: number,
    statusText: string,
  ) {
    super(`HTTP Error ${status} ${statusText}`);
    this.name = "HttpError";
  }
}

/**
 * Performs a GET through the shared request queue (bounded concurrency, retry
 * on dropped connections and 5xx but not on client errors), parses JSON, and
 * rewraps any non-abort failure with a caller-supplied prefix.
 */
async function scheduledGetJson<T>(
  url: string,
  signal: AbortSignal | undefined,
  errorPrefix: string,
): Promise<T> {
  try {
    return await scheduleRequest<T>(
      async (sig) => {
        const response = await fetch(url, { signal: sig });
        if (!response.ok) {
          throw new HttpError(response.status, response.statusText);
        }
        return await response.json();
      },
      {
        signal,
        shouldRetry: (error) =>
          error instanceof HttpError ? error.status >= 500 : true,
      },
    );
  } catch (e) {
    if (isAbortError(e)) throw e;
    throw new Error(errorPrefix + e.message);
  }
}

/**
 * Fetches the configuration data for a single item through the shared request
 * queue.
 * @param path - The path of the configuration data.
 * @param signal - Optional AbortSignal to cancel the request (and its retries).
 * @returns A promise that resolves to the configuration data.
 * @throws An AbortError if cancelled, otherwise a wrapped Error on failure.
 */
export async function fetchConfigData(
  path: string,
  signal?: AbortSignal,
): Promise<ConfigData> {
  return scheduledGetJson<ConfigData>(
    APP_CONFIG.config_path + path,
    signal,
    "Error getting config data from the device: ",
  );
}

/**
 * Fetches the list of configuration card paths through the shared request
 * queue.
 * @param signal - Optional AbortSignal to cancel the request (and its retries).
 * @returns A promise that resolves to an array of card paths.
 * @throws An AbortError if cancelled, otherwise a wrapped Error on failure.
 */
export async function fetchConfigPaths(
  signal?: AbortSignal,
): Promise<string[]> {
  return scheduledGetJson<string[]>(
    APP_CONFIG.config_path + "?cards",
    signal,
    "Error getting the configuration card list from the device: ",
  );
}

/**
 * Saves the configuration data to the server.
 * @param path - The path of the configuration data.
 * @param string - The configuration data to be saved.
 * @throws An error if there is an issue saving the configuration data.
 */
export async function saveConfigData(
  path: string,
  data: string,
  errorHandler: (e: Error) => void,
  contentType: string = "application/json",
): Promise<boolean> {
  try {
    const response = await fetch(APP_CONFIG.config_path + path, {
      method: "PUT",
      headers: {
        "Content-Type": contentType,
      },
      body: data,
    });
    if (!response.ok) {
      errorHandler(
        Error(`HTTP Error ${response.status} ${response.statusText}`),
      );
      return false;
    }
  } catch (e) {
    errorHandler(Error(`Error saving config data to server: ${e.message}`));
    return false;
  }
  return true;
}
