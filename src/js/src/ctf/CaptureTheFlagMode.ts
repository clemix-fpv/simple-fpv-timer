import { Mode } from "../SimpleFpvTimer.js";
import { RaceConfigPage } from "../race/tabs/Config.js";
import { SignalPage } from "../race/tabs/Signal.js";
import { CtfConfigPage } from "./tabs/CtfConfigPage.js";



export class CaptureTheFlagMode extends Mode {

    constructor(name: string) {
        super(name);

        this.pages.push(new CtfConfigPage())
        this.pages.push(new SignalPage())
    }
}
