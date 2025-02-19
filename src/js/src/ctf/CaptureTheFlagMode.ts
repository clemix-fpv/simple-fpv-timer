import { Mode } from "../SimpleFpvTimer.js";
import { RaceConfigPage } from "../race/tabs/Config.js";
import { SignalPage } from "../race/tabs/Signal.js";
import { CtfScoreingPage } from "./tabs/CtfScoreing.js";



export class CaptureTheFlagMode extends Mode {

    constructor(name: string) {
        super(name);

        this.pages.push(new CtfScoreingPage());
        this.pages.push(new RaceConfigPage());
        this.pages.push(new SignalPage());
    }
}
