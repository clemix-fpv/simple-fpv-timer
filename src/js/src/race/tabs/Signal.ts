import uPlot, { Options, Axis, AlignedData } from "../../lib/uPlot.js";
import van from "../../lib/van-1.5.2.js"
import { Notifications } from "../../Notifications.js";
import { Config, Lap, Page, Player, RssiData, RssiEvent } from "../../SimpleFpvTimer.js";
import { $, enumToMap, format_ms } from "../../utils.js";
//import uPlot  from "../../lib/uPlot.js"

const {button, div, pre, select, option} = van.tags

class RssiQ {
    freq: number;
    data: Array<RssiData>;

    uplot_index: number;

    constructor(freq: number, data?: RssiData) {
        this.freq = freq;
        this.data = new Array();
        if (data)
            this.data.push(data);
    }

    public addData(data: RssiData) {
        this.data.push(data);
    }

    static fromJSON(json: any) {
        if (json.freq) {
            var rssiQ = new RssiQ(json.freq);
            if (json.data){
                for (let i = 0; i < json.data.length; i++) {
                    rssiQ.addData(json.data[i]);
               }
            }
            return rssiQ;
        }
        return null;
    }

    public expire(time: number) {
        while(this.data.length > 0 && this.data[0].t < time) {
            this.data.shift();
        }
    }

    public expireOlderThen(seconds: number) {
        const time = Date.now() - (seconds * 1000);
        this.expire(time);
    }
}


function svg(svg_path: string) {
    const svg = document.createElementNS("http://www.w3.org/2000/svg","svg");
    svg.setAttribute("width", "16");
    svg.setAttribute("height", "16");
    svg.setAttribute("viewBox", "0 0 16 16");
    svg.classList.add("bi");
    svg.classList.add("bi-pause");
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("d", svg_path);
    svg.appendChild(path);
    return svg;
}

function svg_pause() {
    return svg("M6 3.5a.5.5 0 0 1 .5.5v8a.5.5 0 0 1-1 0V4a.5.5 0 0 1 .5-.5m4 0a.5.5 0 0 1 .5.5v8a.5.5 0 0 1-1 0V4a.5.5 0 0 1 .5-.5");
}

function svg_play() {
    return svg("M10.804 8 5 4.633v6.734zm.792-.696a.802.802 0 0 1 0 1.392l-6.363 3.692C4.713 12.69 4 12.345 4 11.692V4.308c0-.653.713-.998 1.233-.696z");
}

export class SignalPage extends Page {
    root: HTMLElement;
    uplot: uPlot;
    dataq: Array<RssiQ>;
    max_seconds: number; /* Number of seconds to display */
    show_raw_rssi: boolean;
    max_storage_seconds: number; /* number of seconds in dataq */
    date_offset: number;
    cfg: Config;
    update_uplot: boolean;
    players: Player[];


    getSeriesColor(i: number) {
        switch(i%8) {
            case 0: return "#03fce3";
            case 1: return "#fcba03";
            case 2: return "#fc037b";
            case 3: return "#fc3103";
            case 4: return "#00ff00";
            case 5: return "#0000ff";
            case 6: return "#ff00ff";
            case 7: return "#124511";
            default:
            return "red";
        }
    }

    updateGraph(data): uPlot {
        if (this.uplot) {
            this.uplot.setData(data);
            this.uplot.setSize({width: this.getDom().offsetWidth, height: 600});
            return this.uplot;
        }
    }

    getGraph(): uPlot {
        if (this.uplot)
            return this.uplot;

        const opts: Options = {
            title: "",
            width: 1048,
            height: 600,
            scales: {
                x: {
                    //time: false,
                    time: true,
                },
                y: {
                    auto: false,
                    range: [0, 1300],
                },
            },
            series: [
                {
                    label: "time:",
                },
            {
                stroke: "#70dba4",
                label: "peak",
            },
            {
                stroke: "#dbc270",
                label: "enter",
            },
            {
                stroke: "#3af51d",
                label: "leave",
            }
            ],
            axes: [
                {
                    label: "X Axis Label",
                    labelSize: 20,
                },
                {
                    space: 50,
                    side: 1,
                    label: "RSSI",
                    labelGap: 8,
                    labelSize: 8 + 12 + 8,
                    stroke: "grey",
                }
            ],
        };

        //for (let i = 0; i < this.dataq.length; i++) {
        //    const element = this.dataq[i];
        //    opts.series[i+1] = {
        //        label: `freq: ${element.freq}`,
        //        stroke: this.getSeriesColor(i)
        //    }
        //}

        this.uplot = new uPlot(opts, [
            [0,1,3],
            [1,1,1],
            [1,1,1],
            [1,1,1],
        ] , this.getDom());

        return this.uplot;
    }

    private async sendRssiUpdate(enabled: boolean) {
        this.update_uplot = enabled;
        const url = "/api/v1/rssi/update";
        const v = enabled ? "1" : "0";
        await fetch(url, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json; charset=utf-8'
                },
                body: `{"enable": ${v}}`
            });
    }

    private async getRssiUpdate(func: CallableFunction) {
        const url = "/api/v1/rssi/update";
        const response = await fetch(url, {
                method: 'GET',
            });
        if (!response.ok) {
            Notifications.showError({msg: `Failed to get rssi update ${response.status}`})
        } else {
            const json = await response.json();
            func(!!json.enable);
        }
    }

    getDom(): HTMLElement {
        if (!this.root) {
            const btn_rssi_update = button({class: "btn btn-secondary",
                id: "btn_signal_pause",
                type: "button",
                onclick: () => {
                    this.update_uplot = ! this.update_uplot;
                    if (this.update_uplot) {
                        this.sendRssiUpdate(true);
                        this.onNextValues();
                        $("btn_signal_pause").replaceChildren(svg_pause());
                    } else {
                        this.sendRssiUpdate(false);
                        $("btn_signal_pause").replaceChildren(svg_play());
                    }
                }},
                svg_play()
            );
            this.root = div(
                div({class: "input-group flex-nowrap"},
                    btn_rssi_update,
                    select({class: "form-select", id: "select_signal_duration",
                        onchange: () => {
                            var i = $("select_signal_duration") as HTMLSelectElement;
                            this.max_seconds = Number(i.value);
                            this.onNextValues();
                        }},
                        option({value: 30}, "30s"),
                        option({value: 60, selected: true}, "60s"),
                        option({value: 60 * 2}, "2min"),
                        option({value: 60 * 3}, "3min"),
                        option({value: 60 * 5}, "5min"),
                        option({value: 60 * 10}, "10min"),
                        option({value: 60 * 30}, "30min"),
                    ),
                    select({class: "form-select", id: "select_signal_raw",
                        onchange: () => {
                            var i = $("select_signal_raw") as HTMLSelectElement;
                            this.show_raw_rssi = !!Number(i.value);
                            this.onNextValues();
                        }},
                        option({value: 0, selected: true}, "filtered RSSI"),
                        option({value: 1}, "raw RSSI"),
                    )
                )
            );
            this.getRssiUpdate((enabled: boolean) => {
                console.debug("Enable: " + enabled);
                this.update_uplot = enabled;
                btn_rssi_update.replaceChildren(enabled ? svg_pause(): svg_play())
            })
        }
        return this.root;
    }

    needSeriesUpdate(): boolean {
        if (!this.uplot)
            return false;


        return this.uplot.series.length !=
            this.dataq.length
            + 1 /* X-axis */
            + 3 /* peak,enter,leave*/;
    }

    updateSeries() {
        if (!this.uplot)
            return ;

        for (let i = 4; i < this.uplot.series.length; i++) {
            this.uplot.delSeries(i);
        }

        this.dataq.forEach((e,i) => {
            e.uplot_index = i + 4;
            this.uplot.addSeries({
                        label: `freq:${e.freq}`,
                        stroke: this.getSeriesColor(i)
                    }, e.uplot_index);
        });
    }

    data2uplot(start_sec: number, end_sec:number) {

        var peak = 0;
        var enter = 0;
        var leave = 0;
        if (this.cfg) {
            peak = this.cfg.rssi[0].peak;
            enter = this.cfg.rssi[0].offset_enter/ 100 * peak;
            leave = this.cfg.rssi[0].offset_leave/100 * peak;
        }

        var data = Array.from(
            [
                [
                    [start_sec, end_sec],
                    [peak,peak]
                ],
                [
                    [start_sec, end_sec],
                    [enter,enter]
                ],
                [
                    [start_sec, end_sec],
                    [leave,leave]
                ],
            ]);

        var max_time = 0;
        var min_time = Number.MAX_VALUE;
        this.dataq.forEach((q) => {
            const time = new Array<number>();
            const rssi = new Array<number>();
            q.data.forEach((d) => {
                const t = d.t / 1000;
                if (t >= start_sec && t <= end_sec) {
                    max_time = max_time < t ? t : max_time;
                    min_time = min_time > t ? t : min_time;
                    time.push(t);
                    rssi.push( this.show_raw_rssi ? d.r : d.s);
                }
            });
            data.push(Array.from([time, rssi]));
        });

        for (let i = 0; i < 3; i++) {
            data[i][0][0] = min_time;
            data[i][0][1] = max_time;
        }
        return data;
    }

    onNextValues() {
        if (!this.uplot)
            return;


        if (this.needSeriesUpdate()) {
            this.updateSeries();
        }

        const now = Date.now()/1000;
        const data = this.data2uplot(now - this.max_seconds, now);
        this.updateGraph(uPlot.join(data));
    }

    onRssiUpdate(ev: RssiEvent) {
        for(const r of ev.data) {
            if (!this.date_offset) {
                this.date_offset = Date.now() - r.t;
            }

            var q = this.dataq.find((e) => e.freq == ev.freq);
            if (!q){
                q = new RssiQ(ev.freq, r);
                this.dataq.push(q);
            } else {
                q.addData(r);
            }
            q.expire(this.max_storage_seconds)
        }

        this.saveStorage();
        if (this.update_uplot)
            this.onNextValues();
    }

    saveStorage() {
        try {
            localStorage.setItem("SignalStorage", JSON.stringify(this.dataq));
        } catch(e) {
            if (this.max_storage_seconds > 30) {
                console.info(`Storage to small, change history to max ${this.max_storage_seconds}s`)
                this.max_storage_seconds -= 5;
                for(const d of this.dataq) {
                    d.expireOlderThen(this.max_storage_seconds);
                }
            }
        }
    }

    loadStorage() {
        this.dataq = new Array<RssiQ>();
        try {
            var str =  localStorage.getItem("SignalStorage");
            if (str)  {
                const q = JSON.parse(str);
                for (let i = 0; i < q.length; i++) {
                    const rssi = RssiQ.fromJSON(q[i]);
                    if (rssi){
                        this.dataq.push(rssi);
                    }
                }

            }
        }catch(e) {
            this.dataq = new Array<RssiQ>();
        }
    }

    onPlayersUpdate(players: Player[]) {

        this.players = players;
    }

    constructor() {
        super("RSSI");
        this.max_storage_seconds = 60 * 60;
        this.max_seconds = 100;
        this.update_uplot = true;
        this.show_raw_rssi = false;
        this.getDom();
        this.getGraph();
        this.loadStorage();

        document.addEventListener("SFT_RSSI", (e: CustomEventInit<RssiEvent>) => {
            if (e.detail)
                this.onRssiUpdate(e.detail);
        });

        document.addEventListener("SFT_CONFIG_UPDATE", (e: CustomEventInit<Config>) => {
            if (e.detail)
                this.cfg = e.detail;
        });

        document.addEventListener("SFT_PLAYERS_UPDATE", (e: CustomEventInit<Player[]>) => {
            if (e.detail)
                this.onPlayersUpdate(e.detail);
        })
    }
}
