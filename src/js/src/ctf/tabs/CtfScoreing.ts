import uPlot, { Options, Axis, AlignedData } from "../../lib/uPlot.js";
import van from "../../lib/van-1.5.2";
import { Notifications } from "../../Notifications.js";
import { Config, Ctf, Page } from "../../SimpleFpvTimer";
import { $, format_ms, toColor } from "../../utils.js";


const { h1, h3,label, form, select,input,img,fieldset, option, button, div, h5, pre, ul, li, span, a, table, thead, tbody, th, tr,td} = van.tags

export class CtfScoreingPage extends Page {
    root: HTMLElement;
    private _uplot_div: HTMLElement;
    private _legend_div: HTMLElement;
    private _nodes_div: HTMLElement;
    private _buttons_div: HTMLElement;
    uplot: uPlot;
    cfg: Config;


    private plotGraph() {

        // generate bar builder with 60% bar (40% gap) & 100px max bar width
        const _bars80_100   = uPlot.paths.bars({size: [0.8, 100, 10]});

        let opts = {
            width: 800,
            height: 400,
            title: "Overall",
            scales: {
                x: {
                    time: false,
                },
                y: {
                    time: false,
                    range: [0],
                },
            },
            axes: [
                {
                    show: false,
                },
                {
                    show: false,
                    stroke: "#c7d0d9",
                },
            ],
					legend: {
                        show: false,
					},
            series: [
                {
                    show: false,
                },
                {
                    label: "",
                    stroke: "#000001",
                    fill: "#000000A1",
                    paths: uPlot.paths.bars({size: [0.8, 100]}),
                }
            ],
        };

        if (!this.uplot)
            this.uplot = new uPlot(opts, [[0,1,3][0,0,0]], this.uplotDiv);
        return this.uplot;
    }

    get uplotDiv() {
        if (!this._uplot_div)
            this._uplot_div = div();
        return this._uplot_div;
    }
    get legendDiv() {
        if (!this._legend_div)
            this._legend_div = div();
        return this._legend_div;
    }

    get nodesDiv() {
        if (!this._nodes_div)
            this._nodes_div = div();
        return this._nodes_div;
    }


    private updateButtonsDiv(duration_ms: number) {

        var d = div();
        if (duration_ms > 0) {
            d = div({class: "mb-3 input-group",},
                    div({class: 'input-group-prepend'},
                        button({class: "btn btn-outline-danger", type: "button", onclick: () => {
                            this.stopCtf();

                    }}, "Cancel CTF Round"),
                    ),
                    span({class:'input-group-text'}, format_ms(duration_ms)),
                );

        } else {
            d = div({class: "mb-3 input-group",},
                    div({class: 'input-group-prepend'},
                        button({class: "btn btn-outline-success", type: "button", onclick: () => {

                            var duration = ($('input_ctf_duration') as HTMLInputElement).value;
                            this.startCtf(Number(duration));

                    }}, "CTF Start"),
                    ),
                    span({class:'input-group-text'},'with'),
                    input({class: 'form-control', id:'input_ctf_duration', value: "10"}),
                    span({class:'input-group-text'},'minutes duration'),

                );

        }
        this.buttonsDiv.replaceChildren(d);
    }

    async stopCtf() {
        const url = "/api/v1/ctf/stop";
        try{
            const response = await fetch(url, {
                method: 'GET',
            });
            if (!response.ok) {
                Notifications.showError({msg: `Failed to save config ${response.status}`});
            }
        }catch(e){
            console.error(e);
        }
    }

    async startCtf(duration_minutes: number) {
        const url = "/api/v1/ctf/start";
        try{
            const data = {duration_ms: Number(duration_minutes) * 60 * 1000};
            console.debug(data);
            const response = await fetch(url, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json; charset=utf-8'
                },
                body: JSON.stringify(data)
            });
            if (!response.ok) {
                Notifications.showError({msg: `Failed to save config ${response.status}`});
            }
        }catch(e){
            console.error(e);
        }
    }

    get buttonsDiv() {
        if (!this._buttons_div)
            this._buttons_div = div();
            return this._buttons_div;
    }

    getDom() {
        if (! this.root) {
            this.root = div(
                this.buttonsDiv,
                this.nodesDiv,
                this.uplotDiv,
                this.legendDiv,
            );
        }
        return this.root;
    }

    private colorByTeam(team_name: string) : string {
        for (let i = 0; i < this.cfg.rssi.length; i++) {
            const rssi = this.cfg.rssi[i];

            if (rssi.name == team_name) {
                return toColor(rssi.led_color);
            }
        }

        return "#eeeeee";
    }

    private onCtfUpdate(ctf: Ctf) {

        console.debug(ctf);

        const sum = new Map();

        for(const team of ctf.team_names) {
            sum.set(team, 0);
        }

        var max = 0;
        for(const node of ctf.nodes) {
            for (let i = 0; i < node.captured_ms.length; i++) {
                const team_name = ctf.team_names[i];
                const e = node.captured_ms[i];
                const c = sum.get(team_name) + e;
                if (c > max)
                    max = c;
                sum.set(team_name, c);
            }
        }

        while(this.uplot.series.length > 1)
            this.uplot.delSeries(1);

        var i = 0;
        const data = [];
        data[0]=[];
        for (const team_name of sum.keys()) {
            data[0][i] = i+1;
            for (let j = 0; j < i; j++) {
               data[j+1][i] = null;
            }
            data[i+1] = [];
            for(let j=0; j < i; j++) {
                data[i+1][j] = null;
            }
            data[i+1][i] = sum.get(team_name);

            const color = this.colorByTeam(team_name);
            this.uplot.addSeries( {
                label: team_name,
                stroke: color,
                fill: color + "1A",
                paths: uPlot.paths.bars({size: [0.8, 100]}),
            })
            i++;
        }
        data[0].unshift(0);
        data[0].push(i+1);
        for (let i = 1; i < data.length; i++) {
            data[i].unshift(null);
            data[i].push(null);
        }


        console.debug(data);

        this.uplot.setSize({width: this.getDom().offsetWidth, height: 400});
        this.uplot.setScale("y", {min: 0, max: max});
        this.uplot.setData(data)

        /* build legend */

        var d = div();
        for (const [team_name, duration] of sum) {
            var color = this.colorByTeam(team_name);
            var style = `background-color: ${color}A1`;
            d.append(
                div({class: "card-text border rounded ", style: `margin: 3px; `},
                    span({style: `${style}; font-weight: bold; border-right: 1px solid grey; margin: 0px 0px; padding:0 5px; min-width: 80px; display: inline-block; text-shadow: 1px 1px 1px black;`}, team_name),
                    span({style: `padding: 0px 5px`}, format_ms(duration)))
            );
        }
        this.legendDiv.replaceChildren(d);

        var d = div({style: "margin:10px; display: block;"});
        for(const node of ctf.nodes) {
            color = "#eee";
            if (node.current >= 0 && node.current < ctf.team_names.length) {
                var team = ctf.team_names[node.current];
                color = this.colorByTeam(team);
            }

            d.append(
                span({class: "border rounded", style: `font-weight: bold; margin: 10px; padding: 10px; text-shadow: 1px 1px 1px black; background-color: ${color}A1`},
                    a({href:'http://'+ node.ipv4 , style: "color: #fff; font-weight: bold; text-decoration: none; text-shadow: 1px 1px 1px black;", target: "_blank"},node.name )
                )
            );
        }
        this.nodesDiv.replaceChildren(d);

        this.updateButtonsDiv(ctf.time_left_ms);
    }


    constructor() {
        super("CTF");
        this.getDom();
        this.plotGraph();

        document.addEventListener("SFT_CTF_UPDATE", (e: CustomEventInit<Ctf>) => {
            if (e.detail)
                this.onCtfUpdate(e.detail);
        });

        document.addEventListener("SFT_CONFIG_UPDATE", (e: CustomEventInit<Config>) => {
            if (e.detail)
                this.cfg = e.detail;
        });


    }
}
