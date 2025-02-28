import { SimpleFpvTimer, Mode, Page, RssiEvent, ConfigGameMode, Config, ConfigNodeMode } from "./SimpleFpvTimer.js"
import van from "./lib/van-1.5.2.js"
import "./lib/bootstrap.bundle.js"
import { RaceMode } from "./race/RaceMode.js"
import { Notifications } from "./Notifications.js"
import { TimeSync } from "./TimeSync.js"
import { CaptureTheFlagMode } from "./ctf/CaptureTheFlagMode.js"
import { TestuPlot } from "./race/tabs/TestuPlot.js"
import { SpectrumMode } from "./spectrum/SpectrumMode.js"
import { NodePage } from "./NodePage.js"
import { ConfigForm, ElrsConfigGroup, RaceConfigPage, VisibleElements, WifiConfigGroup } from "./race/tabs/Config.js"
import { PlayersPage } from "./race/tabs/Players.js"
import { LapsPage } from "./race/tabs/Laps.js"

const {button, div, pre,h3} = van.tags



class DebugPage extends Page {
    root: HTMLElement;
    _cfg: HTMLElement;
    _rssi: HTMLElement;
    _ctf: HTMLElement;

    get cfg(): HTMLElement {
        if (!this._cfg) {
            this._cfg = div("CONFIG:");
        }
        return this._cfg;
    }

    get rssi(): HTMLElement {
        return this._rssi ? this._rssi : (this._rssi = div("RSSI"));
    }
    get ctf(): HTMLElement {
        return this._ctf ? this._ctf : (this._ctf = div("ctf"));
    }

    getDom(): HTMLElement {
        if (!this.root) {
            this.root = div(
                this.cfg,
                this.rssi,
                this.ctf,
            );
        }
        return this.root;
    }

    constructor() {
        super("DEBUG");
        this.getDom();

        document.addEventListener("SFT_RSSI", (e: CustomEventInit<RssiEvent>) => {
            this.rssi.replaceChildren(
                h3("Last RSSI message:" + Date.now()),
                pre(JSON.stringify(e.detail, undefined, 2))
            );
        });

        document.addEventListener("SFT_CONFIG_UPDATE", (e: CustomEventInit<Config>) => {
            this.cfg.replaceChildren(
                h3("Config"),
                pre(e.detail.toJsonString(2))
            );
        });

        document.addEventListener("SFT_CTF_UPDATE", (e: CustomEventInit<Ctf>) => {
            this.ctf.replaceChildren(
                h3("Last CTF message:" + Date.now()),
                pre(JSON.stringify(e.detail, undefined, 2))
            );
        });


    }
}

class ServerElementsRace extends VisibleElements {
    init(
        cfg: Config,
        hidden?: Array<string>,
        hidden_groups?: Array<string>,
        _rssi_min?:number,
        _rssi_max?: number) {
        super.init(cfg, hidden, hidden_groups, 1, 8);

        this.rssi_label = "Players";
        this.hidden.push('node_name');
        this.hidden.push('node_mode');
        this.hidden.push('rssi[0].led_color');
        this.hidden.push('rssi[1].led_color');
        this.hidden.push('rssi[2].led_color');
        this.hidden.push('rssi[3].led_color');
        this.hidden.push('rssi[4].led_color');
        this.hidden.push('rssi[5].led_color');
        this.hidden.push('rssi[6].led_color');
        this.hidden.push('rssi[7].led_color');
        this.hidden.push('ctrl_ipv4');
        this.hidden_groups.push(ElrsConfigGroup.name)
        this.hidden_groups.push(WifiConfigGroup.name)
    }
}

class ServerRaceConfigPage extends Page {
    root: HTMLElement;

    getDom(): HTMLElement {
        if (! this.root) {
            this.root = div();
        }

        var cfg = new Config();
        cfg.update((cfg: Config) => {
            this.root.replaceChildren(new ConfigForm(cfg, new ServerElementsRace(cfg)).draw());
        });
        return this.root;
    }

    constructor() {
        super("Config");
    }
}

class ServerRaceMode extends Mode {

    constructor(name: string) {
        super(name);

        this.pages.push(new PlayersPage())
        this.pages.push(new LapsPage())
        this.pages.push(new ServerRaceConfigPage())
        this.pages.push(new NodePage())
        this.pages.push(new DebugPage())
    }
}

const raceMode = new ServerRaceMode ("Race");
const ctfMode = new CaptureTheFlagMode("CTF");
ctfMode.addPage(new NodePage());
ctfMode.addPage(new DebugPage());

const spectrumMode = new SpectrumMode("Spectrum");
spectrumMode.addPage(new NodePage());
spectrumMode.addPage(new DebugPage());

const app = new SimpleFpvTimer();
app.addMode(ConfigGameMode.RACE, raceMode);
app.addMode(ConfigGameMode.CTF, ctfMode);
app.addMode(ConfigGameMode.SPECTRUM, spectrumMode);

van.add(document.body, app.getDom());
const notifications = new Notifications();

TimeSync.instance().sync_time(null);
setTimeout(() => {
    console.debug("TimeSync offset: " + TimeSync.getOffset());
    }, 2000);

