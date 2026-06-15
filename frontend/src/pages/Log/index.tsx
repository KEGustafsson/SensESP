import { AppPage } from "pages/AppPage";
import { type JSX } from "preact";
import { PageContents } from "../PageContents";
import { PageHeading } from "../PageHeading";
import { LogLines } from "./LogLines";

export function LogPage(): JSX.Element {
  return (
    <AppPage>
      <PageHeading title="Device Log" />
      <PageContents>
        <LogLines />
      </PageContents>
    </AppPage>
  );
}
