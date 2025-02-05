import { Mode } from "../SimpleFpvTimer.js";
import { SignalPage } from "../race/tabs/Signal.js";
import { SpectrumConfigPage } from "./tabs/SpectrumConfigPage.js";



export class SpectrumMode extends Mode {

    constructor(name: string) {
        super(name);

        this.pages.push(new SpectrumConfigPage())
        this.pages.push(new SignalPage())
    }
}
