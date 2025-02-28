import { Page } from "./SimpleFpvTimer";
import { TimeSync } from "./TimeSync";
import van from "./lib/van-1.5.2.js"
import { format_ms } from "./utils";
const {button, div, pre,h3, h5, a} = van.tags


class Node {
    ipaddr: string;
    name: string;
    last_seen:number;
}

class Nodes {
    public async update(func: CallableFunction) {
        const url = "/api/v1/nodes";
        try {
            const response = await fetch(url);
            if (!response.ok) {
                throw new Error(`Response status: ${response.status}`);
            }

            const json = await response.json();

            if (json.nodes) {
                document.dispatchEvent(
                    new CustomEvent("SFT_NODES_UPDATE", {detail: json.nodes})
                );
                func(json.nodes);
            }
        } catch (error) {
            console.error(error);
        }
    }

}

export class NodePage extends Page {
    root: HTMLElement;
    _nodes: HTMLElement;

    get nodes(): HTMLElement {
        if (!this._nodes) {
            this._nodes = div("Nodes:");
        }
        return this._nodes;
    }

    private draw_node(node: Node) : HTMLElement {
        const now = Date.now();
        const last_seen = now - (node.last_seen + TimeSync.getOffset());
        var color = "";
        if (last_seen > 30000) {
            color = "color: red;"
        }
        return div({class:"card", style: "margin: 5px;"},
            h5({class: "card-header"}, node.name,
                a({class: "btn btn-secondary btn-sm",
                    style: "float:right", role: "button",
                    href: "http://" + node.ipaddr }, "⚙️" /*"&#9881;"*/)
                ),
            div({class: "card-text", style: `padding: 3px; font-weight: bold;${color}`},
                "Last-seen:  ", format_ms(last_seen)),
            div({class: "card-text", style: "padding: 3px; font-weight: bold;"}, "IP:  ", node.ipaddr)
        );
    }

    update_nodes(nodes: Node[]) {

        this.nodes.replaceChildren(
           h3("Nodes:"),
        );

        for (const n of nodes) {
            this.nodes.appendChild(this.draw_node(n));
        }
    }

    requestNodes() {
        new Nodes().update( (nodes: Node[]) => {
            this.update_nodes(nodes);
        });

        setTimeout(()=> {
            this.requestNodes();
        }, 5000);
    }

    getDom(): HTMLElement {
        if (!this.root) {
            this.root = div(
                this.nodes
            );
        }

        return this.root;
    }

    constructor() {
        super("Nodes");
        this.getDom();
        this.requestNodes();
    }
}


