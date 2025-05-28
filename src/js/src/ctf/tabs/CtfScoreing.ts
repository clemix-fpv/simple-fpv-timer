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

function draw_pie_chart(canvas : HTMLCanvasElement,
        data: {color: string; ms:number; arc_start?:number, arc_end?:number}[]) {
    const { width, height } = canvas.getBoundingClientRect();
    const context = canvas.getContext('2d');
    context.clearRect(0, 0, canvas.width, canvas.height);

    const min = 180;
    const max = 360;
    const arc_max = max - min;

    const radius = (width > height ? height : width) - 20;
    const x = width / 2;
    const y = height - 10;

    console.debug(`w:${width} h:${height} r:${radius} x:${x} y:${y}`);

    var sum_ms = 0;
    data.forEach((e) => {sum_ms += e.ms });

    var arc = min;
    for ( const d of data) {
        const arc_add = (d.ms / sum_ms) * arc_max;
        d.arc_start = arc;
        arc += arc_add;
        d.arc_end = arc;
    }
    data.sort((a,b) => {
        return a.ms - b.ms;
    })


    for ( const d of data) {
        if (d.ms == 0)
            continue;
        var arc_mid = (d.arc_end - d.arc_start) /  2  + d.arc_start;
        context.shadowBlur = 10;
        context.shadowColor = "black";
        context.beginPath();
        context.moveTo(x, y);
        context.arc(x, y, radius, toRad(d.arc_start), toRad(d.arc_end), false);
        context.closePath();
        context.lineWidth = 2;
        context.stroke();

        const gx = x + radius * Math.cos(toRad(arc_mid));
        const gy = y + radius * Math.sin(toRad(arc_mid));
        const grad = context.createLinearGradient(x,y, gx,gy);
        grad.addColorStop(0, getLightColorHex(d.color, 0.8));
        grad.addColorStop(1, d.color);

        context.fillStyle = grad;
        context.fill();
        context.strokeStyle = d.color;
        context.stroke()

        for (var i = d.arc_start; i < d.arc_end; i+= 5) {
            context.beginPath();
            context.moveTo(x, y);
            const lx = x + radius * Math.cos(toRad(i));
            const ly = y + radius * Math.sin(toRad(i));
            context.lineTo(lx, ly);
            context.shadowBlur = 0;
            context.shadowColor = "";

            const gx = x + radius * Math.cos(toRad(arc_mid));
            const gy = y + radius * Math.sin(toRad(arc_mid));
            const grad = context.createLinearGradient(x,y, gx,gy);
            grad.addColorStop(1, getLightColorHex(d.color, 0.8));
            grad.addColorStop(0.8, d.color);
            grad.addColorStop(0.5, d.color);
            grad.addColorStop(0, getLightColorHex(d.color, 0.8));
            context.strokeStyle = grad;
            context.stroke()
        }
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
            this._canvas_div = canvas({width:800, height:400});
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
        for (const team_name of Array.from(sum.keys()).sort()) {
            data.push({
                color: this.colorByTeam(team_name),
                ms: sum.get(team_name),
                team: team_name
            });
        }

        var size = this.getDom().offsetWidth;
        if (window.innerWidth < size) {
            size = window.innerWidth;
        }
        var width = 800;
        var height = 400;
        if (size < 800) {
            width = size;
            height = width/2;
        }
        this.canvasDiv.setAttribute("width", width.toString());
        this.canvasDiv.setAttribute("height", height.toString());
        draw_pie_chart(this.canvasDiv, data);

        /* build legend */

        var html_div = div({style: 'margin: auto auto;'});
        data.sort((a, b)=>{return b.ms - a.ms})

        var rank = 1;
        for (const elem of data) {
            var style = `background-color: ${elem.color}A1`;
            html_div.append(
                div({class: "card-text border rounded ", style: `margin: 3px;`},
                    span({style: `${style}; font-weight: bold; border-right: 1px solid grey; margin: 0px 0px; padding:0 5px; min-width: 120px; display: inline-block; text-shadow: 1px 1px 1px black;`},
                    (rank == 1)? "🏆 " /* "&#127942;" || "&#129351;"*/ :
                    (rank == 2)? "🥈 " /*"&#129352;"*/ :
                    (rank == 3)? "🥉 ": "",
                    elem.team
                    ),
                    span({style: `padding: 0px 5px`}, format_ms(elem.ms))),

            );
            rank++;
        }
        this.legendDiv.replaceChildren(html_div);

        var html_div = div({style: "margin:10px; display: block;"});
        for(const node of ctf.nodes) {
            var color = "#eee";
            if (node.current >= 0 && node.current < ctf.team_names.length) {
                var team = ctf.team_names[node.current];
                color = this.colorByTeam(team);
            }

            html_div.append(
                span({class: "border rounded", style: `font-weight: bold; margin: 10px; padding: 10px; text-shadow: 1px 1px 1px black; background-color: ${color}A1`},
                    a({href:'http://'+ node.ipv4 , style: "color: #fff; font-weight: bold; text-decoration: none; text-shadow: 1px 1px 1px black;", target: "_blank"},node.name )
                )
            );
        }
        this.nodesDiv.replaceChildren(html_div);

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
