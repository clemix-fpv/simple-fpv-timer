import van from "../../lib/van-1.5.2";
import { Notifications } from "../../Notifications.js";
import { Config, Ctf, Page } from "../../SimpleFpvTimer";
import { $, format_ms, toColor } from "../../utils.js";


const { canvas, input, button, div, span, a} = van.tags

function toRad (x: number) {
    return (x*Math.PI)/180;
}

function getLightColorHex(hexColor: string, factor = 0.6): string | null {
    /**
   * Takes a color in hex notation (e.g., "#RRGGBB") and returns a lighter version of it.
   */
    if (!hexColor || typeof hexColor !== 'string' || !hexColor.startsWith('#') || hexColor.length !== 7) {
        return null;
    }

    try {
        var ret = "#";
        for (var i =0; i < 3; i++) {
            const hex = hexColor.substring(i*2 + 1, i*2 + 3);
            let num = parseInt(hex, 16);
            num = Math.round(num + (255 - num) * factor);
            ret += Math.max(0, Math.min(255, num)).toString(16).padStart(2, '0');
        }
        return ret;

    } catch (error) {
        return null;
    }
}

function draw_pie_chart(canvas : HTMLCanvasElement, data:any) {
    const { width, height } = canvas.getBoundingClientRect();
    const context = canvas.getContext('2d');
    const min = 180;
    const max = 360;
    const arc_max = max - min;

    const x = width/2;
    const y = height;
    const radius = (width > height? height : width) -8;

    var sum = 0;
    for ( const d of data) {
        sum += d.value;
    }

    var arc = min;
    for ( const d of data) {
        context.shadowBlur = 10;
        context.shadowColor = "black";
        const arc_add = (d.value / sum) * arc_max;
        context.beginPath();
        context.moveTo(x, y);
        context.arc(x, y, radius, toRad(arc), toRad(arc + arc_add), false);
        context.closePath();
        context.lineWidth = 2;
        context.stroke();

        const gx = x + radius * Math.cos(toRad(arc + arc_add/2));
        const gy = y + radius * Math.sin(toRad(arc + arc_add/2));
        const grad = context.createLinearGradient(x,y, gx,gy);
        grad.addColorStop(0, getLightColorHex(d.color, 0.8));
        grad.addColorStop(1, d.color);

        context.fillStyle = grad;
        context.fill();
        context.strokeStyle = d.color;
        context.stroke()

        for (var i = arc; i < arc + arc_add; i+= 5) {
            context.beginPath();
            context.moveTo(x, y);
            const lx = x + radius * Math.cos(toRad(i));
            const ly = y + radius * Math.sin(toRad(i));
            context.lineTo(lx, ly);
            context.shadowBlur = 0;
            context.shadowColor = "";

            const gx = x + radius * Math.cos(toRad(arc + arc_add/2));
            const gy = y + radius * Math.sin(toRad(arc + arc_add/2));
            const grad = context.createLinearGradient(x,y, gx,gy);
            grad.addColorStop(1, getLightColorHex(d.color, 0.8));
            grad.addColorStop(0.8, d.color);
            grad.addColorStop(0.5, d.color);
            grad.addColorStop(0, getLightColorHex(d.color, 0.8));
            context.strokeStyle = grad;
            context.stroke()
        }
        arc += arc_add;
    }
}

export class CtfScoreingPage extends Page {
    root: HTMLElement;
    private _canvas_div: HTMLCanvasElement;
    private _legend_div: HTMLElement;
    private _nodes_div: HTMLElement;
    private _buttons_div: HTMLElement;
    cfg: Config;


    get canvasDiv() {
        if (!this._canvas_div)
            this._canvas_div = canvas({width: 500, height:500});
        return this._canvas_div;
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
                this.canvasDiv,
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

        var data = [];
        for (const team_name of sum.keys()) {
            data.push({
                color: this.colorByTeam(team_name),
                value: sum.get(team_name)
            });
        }

        console.debug(data);
        this.canvasDiv.setAttribute("width", this.getDom().offsetWidth.toString());
        this.canvasDiv.setAttribute("height", "400");
        draw_pie_chart(this.canvasDiv, data);

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
