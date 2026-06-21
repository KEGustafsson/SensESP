import { fetchConfigData, fetchConfigPaths } from "./configAPIClient";

const fetchMock = jest.fn();
globalThis.fetch = fetchMock as unknown as typeof fetch;

const makeResponse = (
  status: number,
  body: unknown,
  statusText = "",
): Response =>
  ({
    ok: status >= 200 && status < 300,
    status,
    statusText,
    json: async () => body,
  }) as unknown as Response;

const CARD = {
  config: {},
  schema: {},
  title: "Card",
  description: "",
  requires_restart: false,
};

beforeEach(() => {
  fetchMock.mockReset();
  jest.spyOn(Math, "random").mockReturnValue(0); // collapse backoff to 0ms
});

afterEach(() => jest.restoreAllMocks());

describe("fetchConfigData", () => {
  it("retries a 503 then resolves with the parsed config", async () => {
    fetchMock
      .mockResolvedValueOnce(makeResponse(503, "busy", "Service Unavailable"))
      .mockResolvedValueOnce(makeResponse(200, CARD, "OK"));

    await expect(fetchConfigData("/X")).resolves.toEqual(CARD);
    expect(fetchMock).toHaveBeenCalledTimes(2);
  });

  it("does not retry a 404 and wraps the error message", async () => {
    fetchMock.mockResolvedValue(makeResponse(404, "nope", "Not Found"));

    await expect(fetchConfigData("/X")).rejects.toThrow(
      /Error getting config data from the device: HTTP Error 404/,
    );
    expect(fetchMock).toHaveBeenCalledTimes(1);
  });

  it("retries a dropped connection then resolves", async () => {
    fetchMock
      .mockRejectedValueOnce(new TypeError("Failed to fetch"))
      .mockResolvedValueOnce(makeResponse(200, CARD, "OK"));

    await expect(fetchConfigData("/X")).resolves.toEqual(CARD);
    expect(fetchMock).toHaveBeenCalledTimes(2);
  });

  it("rethrows an AbortError unwrapped and never fetches", async () => {
    const ac = new AbortController();
    ac.abort();

    await expect(fetchConfigData("/X", ac.signal)).rejects.toMatchObject({
      name: "AbortError",
    });
    expect(fetchMock).not.toHaveBeenCalled();
  });
});

describe("fetchConfigPaths", () => {
  it("returns the card path array", async () => {
    fetchMock.mockResolvedValue(makeResponse(200, ["/A", "/B"], "OK"));

    await expect(fetchConfigPaths()).resolves.toEqual(["/A", "/B"]);
  });
});
