import { Mode } from "../SimpleFpvTimer.js";
import { RaceConfigPage } from "../race/tabs/Config.js";
import { SignalPage } from "../race/tabs/Signal.js";

export class SpectrumMode extends Mode {

    constructor(name: string) {
        super(name);

        this.pages.push(new RaceConfigPage());
        this.pages.push(new SignalPage());
    }
}
