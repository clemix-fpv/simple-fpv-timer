import { SimpleFpvTimer, Mode, Page, RssiEvent, ConfigGameMode, Config } from "./SimpleFpvTimer.js"
import van from "./lib/van-1.5.2.js"
import "./lib/bootstrap.bundle.js"
import { RaceMode } from "./race/RaceMode.js"
import { Notifications } from "./Notifications.js"
import { TimeSync } from "./TimeSync.js"
import { CaptureTheFlagMode } from "./ctf/CaptureTheFlagMode.js"
import { TestuPlot } from "./race/tabs/TestuPlot.js"
import { SpectrumMode } from "./spectrum/SpectrumMode.js"

const {button, div, pre,h3} = van.tags


class DebugPage extends Page {
    root: HTMLElement;
    _cfg: HTMLElement;
    _rssi: HTMLElement;
    _ctf: HTMLElement;
    _settings: HTMLElement;

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

    private async update_settings() {
        const url = "/api/v1/settings";
            const response = await fetch(url, {
                    method: 'GET',
                });
            if (!response.ok) {
                Notifications.showError({msg: `Failed to get rssi update ${response.status}`})
            } else {
                    const json = await response.json();
                    this._settings.replaceChildren(
                        h3("/api/v1/settings (click for update)"),
                        pre(JSON.stringify(json, undefined, 2))
                    );
            }

    }

    get settings(): HTMLElement {
        if (!this._settings) {
            this._settings = div({
                onclick: () => {
                    this.update_settings();
                    }
            }, h3("/api/v1/settings (click for update)"));
        }
        return this._settings;
    }

    getDom(): HTMLElement {
        if (!this.root) {
            this.root = div(
                this.settings,
                this.cfg,
                this.rssi,
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
                pre(JSON.stringify(e.detail, undefined, 2))
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



const raceMode = new RaceMode ("Race");
raceMode.addPage(new DebugPage());

const ctfMode = new CaptureTheFlagMode("CTF");
ctfMode.addPage(new DebugPage());

const spectrumMode = new SpectrumMode("CTF");
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

