import van from "./lib/van-1.5.2.js"
import { Notifications } from "./Notifications.js";
import { TimeSync } from "./TimeSync.js";
import { nullOrUndef } from "./utils.js";
const {button, div, pre, ul, li, a} = van.tags

export abstract class Page {
    name: string;
    visible: boolean;

    abstract getDom(): HTMLElement;

    constructor(name: string) {
        this.name = name;
        this.visible = false;
    }
}

export class Mode {
    name: string;
    pages: Page[];
    currentPage: Page;

    public addPage(page: Page) {
        this.pages.push(page);
    }

    public showPage(page: Page, root: HTMLElement) {

        if (!this.pages.find((e) => e == page)) {
            console.error(`Failed: showPage(${page.name}) - page not in this Mode`);
            return;
        }

        if (this.currentPage)
            this.currentPage.visible = false;

        this.currentPage = page;
        page.visible = true;

        root.replaceChildren(page.getDom());
    }

    public getPageByName(name: string) {
        for(const p of this.pages) {
            if (p.name == name){
                return p;
            }
        }
        return null;
    }

    constructor(name: string) {
        this.name = name;
        this.pages = new Array<Page>();
    }
}

export interface WsEvent {
    type: string;
}

export interface RssiData {
    t: number;
    r: number;
    s: number;
    i: boolean;
}

export interface RssiEvent {
    type: string;
    freq: number;
    data: RssiData[];
}

export interface PlayersEvent {
    type: string;
    players: Player[];
}



export interface CtfNode {
    name: string;
    ipv4: string;
    current: number;
    captured_ms: number[];
}

export interface Ctf {
    team_names: string[];
    nodes: CtfNode[];
    time_left_ms: number;
}

export interface CtfEvent {
    type: string;
    ctf: Ctf[];
}

export class RSSIConfig {
    name: string;
    freq: number;
    peak: number;
    filter: number;
    offset_enter: number;
    offset_leave: number;
    calib_max_lap_count: number;
    calib_min_rssi_peak: number;
    led_color: number;
}

export enum ConfigWIFIMode {
    AP = 0,
    STA = 1,
}

export enum ConfigNodeMode {
    CONTROLLER = 0,
    CLIENT = 1,
}

export enum ConfigGameMode {
    RACE = 0,
    CTF = 1,
    SPECTRUM = 2,
}

export class Config {
    rssi: RSSIConfig[];

    game_mode: ConfigGameMode;
    node_name: string;
    node_mode: number;
    ctrl_ipv4: string;
    led_num: number;


    elrs_uid: string;
    osd_x: number;
    osd_y: number;
    osd_format: string;

    wifi_mode: ConfigWIFIMode;
    ssid: string;
    passphrase: string;


    public getValue(name: string): string {
        var matches = name.match(/^rssi\[(\d)\]\.(\w+)$/);
        if (matches !== null) {
            const idx = matches[1];
            const name = matches[2];
            if (this.rssi[idx]) {
                return nullOrUndef(this.rssi[idx][name], "");
            }
        }
        return nullOrUndef(this[name], "");
    }

    public setValues(obj: Object) {
        for (const [k, v] of Object.entries(obj)) {
            var matches = k.match(/^rssi\[(\d)\]\.(\w+)$/);
            if (matches !== null) {
                const idx = matches[1];
                const name = matches[2];
                if (!this.rssi) {
                    this.rssi = new Array<RSSIConfig>();
                }
                if (!this.rssi[idx]) {
                    this.rssi[idx] = new RSSIConfig();
                }
                this.rssi[idx][name] = v;
            } else if(k.match(/^\w+$/)) {
                this[k] = v;
            }
        }
        return this;
    }

    public toJsonString(ident?:number) {
        var obj = new Object();
        for (const [k, v] of Object.entries(this)) {
            if (k === "rssi") {
                for (let i = 0; i < this.rssi.length; i++) {
                    const element = this.rssi[i];
                    for (const [kk, vv] of Object.entries(element)) {
                        obj[`rssi[${i}].${kk}`] = vv;
                    }
                }
            } else {
                obj[k] = v;
            }
        }
        return JSON.stringify(obj, undefined, ident);
    }

    public async update(func: CallableFunction) {
        const url = "/api/v1/settings";
        try {
            const response = await fetch(url);
            if (!response.ok) {
                throw new Error(`Response status: ${response.status}`);
            }

            const json = await response.json();

            if (json.config) {
                this.setValues(json.config);
                document.dispatchEvent(
                    new CustomEvent("SFT_CONFIG_UPDATE", {detail: this})
                );
                func(this);
            }
        } catch (error) {
            console.error(error);
        }
    }

    public async save(func: CallableFunction) {
        const url = "/api/v1/settings";
        try {
            const response = await fetch(url, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json; charset=utf-8'
                },
                body: this.toJsonString()
            });
            if (!response.ok) {
                Notifications.showError({msg: `Failed to save config ${response.status}`})
                func(null);
            } else {

                const json = await response.json();
                if (json.status !== "ok") {
                    Notifications.showError({msg: `Failed to save config ${json.msg}`})
                    func(null);
                } else {
                    if (json.config) {
                        this.setValues(json.config);

                    }
                    document.dispatchEvent(
                        new CustomEvent("SFT_CONFIG_UPDATE", {detail: this})
                    );
                    func(this);
                    Notifications.showSuccess({msg: "Configuration saved!"});
                }
            }

        } catch (error) {
            console.error(error.message);
        }
    }

}

export class Lap {
    id: number;
    duration: number;
    abs_time: number;
    rssi: number;
}

export class Player {
    name: string;
    ipaddr: string;
    laps: Lap[];

    constructor (name:string) {
        this.name = name;
    }

    from(p: Player) {
        this.name = p.name;
        this.ipaddr = p.ipaddr;
        this.laps = p.laps;
       return this;
    }

    /**
     * Sort laps by duration shortes duration on index 0.
     */
    public sortLapsByDuration() {
        this.laps.sort((a,b) => {
            return a.duration - b.duration;
        });
    }

    /**
     * Sort laps by id, lowest first
     */
    public sortLapsById() {
        this.laps.sort((a,b) => {
            return a.id - b.id;
        });
    }

    public patchTime(offset: number) {
        for (const l of this.laps) {
            l.abs_time += offset;
        }
        return this;
    }
}

export class SimpleFpvTimer {

    _currentMode: Mode;
    _modes : Map<ConfigGameMode, Mode>;
    _ws: WebSocket;
    _players: Player[];
    _root: HTMLElement;

    constructor () {
        this._modes = new Map<ConfigGameMode,Mode>();
        this._currentMode = null;

        this.initWebsocket();

        document.addEventListener("SFT_CONFIG_UPDATE", (e: CustomEventInit<any>) => {
            this.onConfigUpdate(e.detail);
        });

        new Config().update(()=> {
            console.debug("GOT CONFIG!");
        });
    }



    private initWebsocket() {
        if (this._ws) {
            this._ws.close();
            this._ws = null;
        }
        const ws_uri = `ws://${window.location.hostname}:${window.location.port}/ws/rssi`;
        const ws = new WebSocket(ws_uri);

        ws.addEventListener("open", () => {
            console.debug('WebSocket: connected ' + ws_uri);
            this._ws.send(JSON.stringify({type: "hello", msg: 'Hello, server!'}));
        });

        ws.addEventListener("message", (ev) => {
            try {
                const json = JSON.parse(ev.data);
                const wsEv = json as WsEvent;

                if (wsEv.type === "rssi") {
                    this.dispatchRSSIUpdateEv(json as RssiEvent);

                } else if (wsEv.type === "players") {
                    this.dispatchPlayersUpdateEv((json as PlayersEvent).players);

                } else  if (wsEv.type === "ctf") {
                    this.dispatchCtfUpdateEv((json as CtfEvent).ctf);
                }

            } catch {
                console.log("WebSocket: Failed to parse json:" + ev.data);
            }
        });

        ws.onclose = () => {
            console.debug('WebSocket: disconnected ' + ws_uri);

            setTimeout(() => {
                this.initWebsocket()
            }, 1000);
        };

        this._ws = ws;
    }

    private onConfigUpdate(cfg: Config) {
        var expected_mode = this._modes.get(Number(cfg.game_mode));

        if (expected_mode != this._currentMode)
            this.changeMode(expected_mode);
    }

    private changeMode(m: Mode) {
        this._currentMode = m;
        this.getDom().replaceChildren(this.drawTabs());
    }
    public addMode(configMode: ConfigGameMode, mode: Mode) {
        this._modes.set(configMode, mode);
    }

    public drawTabs(): HTMLElement {
        // Currently we have tab's as menu
        const tabList = ul({ class: 'nav nav-tabs' }); // Use destructured ul
        const tabContent = div({ class: 'tab-content' }); // Use destructured div

        const mode = this._currentMode;
        if (!mode)
            return div({class: "alert alert-danger"}, "NO MODE AVAILABLE");

        mode.pages.forEach((page, index) => {
            const tabLink = li({ class: 'nav-item' }, [
                a({
                    class: `nav-link ${index === 0 ? 'active' : ''}`,
                    'data-bs-toggle': 'tab',
                    href: `#tab${index + 1}`,
                    onclick: () => {

                        const tabPane = document.getElementById(`tab${index + 1}`);
                        if (tabPane) {
                            mode.showPage(page, tabPane);
                        }
                    }
                }, page.name),
            ]);
            tabList.appendChild(tabLink);

            const tabPane = div({
                class: `tab-pane fade ${index === 0 ? 'show active' : ''}`,
                id: `tab${index + 1}`,
            });

            if (index === 0 ) {
                tabPane.replaceChildren(page.getDom());
            }
            tabContent.appendChild(tabPane);
        });

        return div(tabList, tabContent);
    }

    public getDom(): HTMLElement {
        if (!this._root) {
            this._root = div();
        }
        return this._root;
    }

    private dispatchPlayersUpdateEv(players: Player[]) {
        var p2 = new Array<Player>();
        for(const p of players) {
            const new_p = new Player("").from(p).patchTime(TimeSync.getOffset());
            p2.push(new_p);
        }
        document.dispatchEvent(
            new CustomEvent("SFT_PLAYERS_UPDATE", {detail: p2})
        );
    }

    private dispatchCtfUpdateEv(ctf: Ctf[]) {
        document.dispatchEvent(
            new CustomEvent("SFT_CTF_UPDATE", {detail: ctf})
        );
    }

    private dispatchRSSIUpdateEv(ev: RssiEvent) {
        for (const rssidata of ev.data) {
            rssidata.t += TimeSync.getOffset();
        }

        document.dispatchEvent(
            new CustomEvent("SFT_RSSI", {detail: ev })
        );
    }

    private async requestSettings() {
        const url = "/api/v1/settings";
        try {
            const response = await fetch(url);
            if (!response.ok) {
                throw new Error(`Response status: ${response.status}`);
            }

            const json = await response.json();
            if (json.status && json.status.players) {
                this.dispatchPlayersUpdateEv(json.status.players as Player[]);
            }

            return json;
        } catch (error) {
            console.error(error.message);
        }

    }
}
