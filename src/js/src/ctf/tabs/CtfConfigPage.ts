import van from "../../lib/van-1.5.2.js"
import { ConfigGroup, ElrsConfigGroup, GeneralConfigGroup, RaceConfigPage, RSSIConfigGroup, TabedRssiConfigGroup, WifiConfigGroup } from "../../race/tabs/Config";
import { Config } from "../../SimpleFpvTimer";
import { $ } from "../../utils.js";
const { h3,label, form, select,input,img,fieldset, option, button, div, h5, pre, ul, li, span, a, table, thead, tbody, th, tr,td} = van.tags

export class CtfConfigPage extends RaceConfigPage {

    drawConfig(cfg: Config) : HTMLElement {
        var groups = new Array<ConfigGroup>();

        groups.push(new GeneralConfigGroup(cfg));
        groups.push(new TabedRssiConfigGroup(cfg, "RSSI Config", 1, 8));
        groups.push(new ElrsConfigGroup(cfg));
        groups.push(new WifiConfigGroup(cfg));

        return div(
            form(
                groups.map((g) => {
                    return g.draw();
                }),
                div({class: "mb-3", style: "margin: 5px 3px"},
                    button({class: "btn btn-primary", type: "button", id: "config_submit_btn_id", onclick: () => {
                        var btn = $('config_submit_btn_id') as HTMLButtonElement;
                        btn.disabled = true;
                        btn.classList.add("disabled");

                        var obj = new Config();
                        groups.forEach((g) => {
                            g.getKeyValues(obj);
                        });
                        var cfg = new Config();
                        cfg.setValues(obj);

                        cfg.save((cfg: Config) => {
                            if (cfg)
                                this.onConfigUpdate(cfg);

                            btn.classList.remove("disabled");
                            btn.disabled = false;
                        });
                    }}, "Submit")
                )
            )
    );
    }

    getDom(): HTMLElement {
        if (! this.root) {
            this.root = div();
        }
        var cfg = new Config();
        cfg.update((cfg: Config) => {
            this.onConfigUpdate(cfg);
        });
        return this.root;
    }

    onConfigUpdate(cfg: Config) {
        if (this.root)
            this.root.replaceChildren(this.drawConfig(cfg));
    }

    constructor() {
        super();

    }

}
