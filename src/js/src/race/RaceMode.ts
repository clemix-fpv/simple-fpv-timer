import { Mode } from "../SimpleFpvTimer.js";
import { RaceConfigPage } from "./tabs/Config.js";
import { LapsPage } from "./tabs/Laps.js";
import { PlayersPage } from "./tabs/Players.js";
import { SignalPage } from "./tabs/Signal.js";



export class RaceMode extends Mode {

    constructor(name: string) {
        super(name);

        this.pages.push(new PlayersPage())
        this.pages.push(new LapsPage())
        this.pages.push(new RaceConfigPage())
        this.pages.push(new SignalPage())
    }
}
