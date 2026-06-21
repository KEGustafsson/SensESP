import { fetchConfigPaths } from "common/configAPIClient";
import { isAbortError } from "common/requestQueue";
import { type JSX } from "preact";
import { useEffect, useState } from "preact/hooks";
import { ConfigCard } from "./ConfigCard";

export function ConfigCards(): JSX.Element {
  const [cards, setCards] = useState<string[] | null>(null);
  const [failed, setFailed] = useState<boolean>(false);
  const [reloadToken, setReloadToken] = useState<number>(0);

  useEffect(() => {
    const controller = new AbortController();
    setFailed(false);

    fetchConfigPaths(controller.signal)
      .then((items) => setCards(items))
      .catch((e: Error) => {
        if (isAbortError(e)) return;
        console.warn("Failed to load the configuration card list", e);
        setFailed(true);
      });

    return () => controller.abort();
  }, [reloadToken]);

  if (failed) {
    return (
      <div className="alert alert-danger m-3" role="alert">
        <p className="mb-1">Couldn't reach the device to load its settings.</p>
        <p className="mb-2 small">
          Make sure it's powered on and connected to the network, then try
          again.
        </p>
        <button
          className="btn btn-outline-secondary btn-sm"
          type="button"
          onClick={() => setReloadToken((token) => token + 1)}
        >
          Retry
        </button>
      </div>
    );
  }

  if (cards === null) {
    // Display a spinner while waiting for data. Center the spinner
    // in the page.
    return (
      <div
        className="d-flex align-items-center justify-content-center min"
        style={{ height: "100vh" }}
      >
        <div className="spinner-border" role="status">
          <span className="visually-hidden">Loading...</span>
        </div>
      </div>
    );
  }

  return (
    <div className="vstack gap-4">
      {cards.map((card) => (
        <ConfigCard key={card} path={card} />
      ))}
    </div>
  );
}
