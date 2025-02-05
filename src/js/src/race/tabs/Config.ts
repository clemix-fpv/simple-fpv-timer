import van from "../../lib/van-1.5.2.js"
import { Notifications } from "../../Notifications.js";
import { Config, ConfigGameMode, ConfigWIFIMode, Lap, Page, Player } from "../../SimpleFpvTimer.js";
import { $, enumToMap, format_ms, hide, Images, show, suffix } from "../../utils.js";

const { h3,label, form, select,input,img,fieldset, option, button, div, h5, pre, ul, li, span, a, table, thead, tbody, th, tr,td} = van.tags



export class ConfigElement {
    fieldname: string;
    value: string;
    label: string;
    help: string;

    constructor(cfg: Config, fieldname: string, label: string, help?: string) {
        this.fieldname = fieldname;
        this.value = cfg.getValue(fieldname);
        this.label = label;
        if (help)
            this.help = help;
    }

    inputId() {
        return `id_input_${this.fieldname}`;
    }

    public draw() {

        return div({class: "form-group"},
            fieldset(
                label( {class: "form-label", for: this.inputId()},
                    this.label,
                    (this.help ?
                        a({style: "margin: 0px 5px",
                            "data-toggle": "tooltip",
                            "data-trigger": "click",
                            title: this.help}, img({src: Images.QUESTION_SVG}))
                        : span())
                ),
                input({class: "form-control", name: this.fieldname,
                    id: this.inputId(), type: "text", value: this.value})
            )
        );
    }

    public getValue(): string | null {
        var e = $(this.inputId());
        if (e) {
            var i = e as HTMLInputElement;
            return i.value;
        }
        return this.value;
    }

    public setValue(v: string) {
        this.value = v;
        var e = $(this.inputId());
        if (e) {
            var i = e as HTMLInputElement;
            i.value = v;
        }
    }

    public getKeyValue(obj_r?: Object): Object{
        var obj = new Object();
        var v = this.getValue();
        if (v) {
            obj[this.fieldname] = v;
            if (obj_r)
                obj_r[this.fieldname] = v;
            return obj;
        }
        return obj;
    }
}

export class ConfigColorElement extends ConfigElement {
    public draw() {
        var value = "#000000";
        var v = this.value || 0;
        if (!Number.isNaN(v)) {
            v = v as number;
            var r = (v & 0xff0000) >> 16;
            var g = (v & 0x00ff00) >> 8;
            var b = (v & 0x0000ff);
            value = "#" + r.toString(16).padStart(2, '0') +
                g.toString(16).padStart(2,'0') + b.toString(16).padStart(2,'0');
        }

        return div({class: "form-group"},
            fieldset(
                label( {class: "form-label", for: this.inputId()},
                    this.label,
                    (this.help ?
                        a({style: "margin: 0px 5px",
                            "data-toggle": "tooltip",
                            "data-trigger": "click",
                            title: this.help}, img({src: Images.QUESTION_SVG}))
                        : span())
                ),
                input({class: "form-control form-control-color", name: this.fieldname,
                    id: this.inputId(), type: "color", value: value})
            )
        );
    }

    public getValue(): string | null {
        var v = super.getValue();
        if (!v) return null;
        var m = v.match(/#(\w{2})(\w{2})(\w{2})/ );
        if (m) {
            var r = parseInt(m[1], 16);
            var g = parseInt(m[2], 16);
            var b = parseInt(m[3], 16);
            v = (r << 16 | g << 8 | b).toString();
            return v;
        }
        return v;
    }
}

export class ConfigSelectElement extends ConfigElement {
    options: Map<string,string>;

    constructor(cfg: Config, fieldname: string, label: string, options: Map<string, string>, help?: string) {
        super(cfg, fieldname, label, help);
        this.options = options;
    }


    public draw() {

        var s = select({class: "form-select", name: this.fieldname, id: this.inputId()});
        var v = this.value;
        for (let [key, value] of this.options) {
            van.add(s, option( {value: key, selected: v == key}, value));
        }

        return div({class: "form-group"},
            fieldset(
                label( {class: "form-label", for: this.inputId()},
                    this.label,
                    (this.help ?
                        a({style: "margin: 0px 5px",
                            "data-toggle": "tooltip",
                            "data-trigger": "click",
                            title: this.help}, img({src: Images.QUESTION_SVG}))
                        : span())
                ),
                s
            )
        );
    }

}

export class ConfigGroup {
    label: string;
    elements: ConfigElement[];
    cfg: Config;

    constructor(cfg: Config, label: string) {
        this.label = label;
        this.cfg = cfg;
        this.elements = new Array<ConfigElement>();
    }

    addElement(elem: ConfigElement) : ConfigGroup{
        this.elements.push(elem);
        return this;
    }

    public draw() : HTMLElement {
        return li({class:"list-group-item bg-gradient rounded", style: "margin-bottom: 5px; padding: 5px"},
            h5({style: "text-align: right; margin:0"}, this.label),
            this.elements.map((e) => {
                return e.draw();
            })
        );
    }

    public getKeyValues(obj_r?: Object): Object{
        var obj = new Object();
        for(const e of this.elements){
            for (const [key, value] of Object.entries(e.getKeyValue(obj_r)) ){
                obj[key] = value;
            }
        }
        return obj;
    }
}

class RSSIConfigElement extends ConfigElement {
    constructor(cfg: Config, idx: number, fieldname: string, label: string, help?: string) {
        super(cfg, `rssi[${idx}].${fieldname}`, label, help);
    }
}

export class GeneralConfigGroup extends ConfigGroup {
    constructor(cfg: Config) {
        super(cfg, "General settings");
        this.addElement(new ConfigSelectElement(cfg,
            "game_mode", "Game mode", enumToMap(ConfigGameMode)));
        this.addElement(new ConfigElement(cfg, "led_num", "Number of LEDs"));
    }

}

const freq_map = new Map([
            [ "5658", "R1 (5658)"  ],
            [ "5695", "R2 (5695)"  ],
            [ "5732", "R3 (5732)"  ],
            [ "5769", "R4 (5769)"  ],
            [ "5806", "R5 (5806)"  ],
            [ "5843", "R6 (5843)"  ],
            [ "5880", "R7 (5880)"  ],
            [ "5917", "R8 (5917)"  ],
            ]);

export class RSSITap extends ConfigGroup {
    tablink: HTMLLIElement;
    tabpane: HTMLDivElement;
    index: number;
    id_prefix = "rssi_config_tab_";

    public draw() : HTMLElement {
        return div(
            this.elements.map((e) => {
                return e.draw();
            })
        );
    }

    public getTabLink() {
        if (!this.tablink)
            this.tablink = li({ class: 'nav-item' }, [
                a({
                    class: `nav-link`,
                    'data-bs-toggle': 'tab',
                    href: `#${this.id_prefix}${this.index}`,
                }, this.label),
            ]);
        return this.tablink;
    }

    public getTabPane() {
        if (!this.tabpane)
            this.tabpane = div({
            class: `tab-pane fade}`,
            id: `${this.id_prefix}${this.index}`} ,
            this.draw()
        );
        return this.tabpane;
    }

    public selectTab() {
        if (document.body.contains(this.getTabLink())) {
            this.getTabLink().firstChild.click();

        }
    }

    public initFromOther(other: RSSITap) {
        var fields = [
            'peak',
            'filter',
            'offset_leave',
            'offset_enter',
            'calib_max_lap_count',
            'calib_min_rssi_peak'
        ];

        for(const other_e of other.elements){
            for(const my of this.elements) {
                const my_suffix = suffix(my.fieldname);
                const other_suffix = suffix(other_e.fieldname);
                if (my_suffix == other_suffix && fields.indexOf(my_suffix) >= 0) {
                    my.setValue(other_e.getValue());
                }
            }
        }
    }

    public getValue(fieldname: string): string | null {

        fieldname = `rssi[${this.index}].${fieldname}`;
        for(const e of this.elements) {
            if (e.fieldname == fieldname) {
                return e.getValue();
            }
        }
        return null;
    }

    constructor(cfg: Config, idx: number) {
        super(cfg, "Player settings");
        this.index = idx;
        this.label = (idx + 1).toString();


        this.addElement(new RSSIConfigElement(cfg, idx, "name", "Name"));
        this.addElement(new ConfigSelectElement(cfg, `rssi[${idx}].freq`,
            "Frequency", freq_map));

        this.addElement(new ConfigColorElement(cfg, `rssi[${idx}].led_color`, "LED color"));

        [{
            label: "RSSI Peak value (default: 1100)",
            name: "peak",
            help: "The expected RSSI maximum, when the drone fly through the gate."
        },
        {
            name: "filter",
            label: "Filter",
            help: "Smooth the RSSI input signals. Range is 1-100, a value near" +
                " to 1 smooth more, a value of 100 keeps the raw RSSI value."
        },
        {
            label: "Offset enter (in %)",
            name: "offset_enter",
            help: "The percentage of 'RSSI-Peak' to count a drone as entered" +
                " the gate. The range is 50-100 (Default: 80)"
        },
        {
            label: "Offset leave (in %)",
            name: "offset_leave",
            help: "The percentage of 'RSSI-Peak' to count a drone as leaved the"+
                " gate. The range is 50-100 (Default: 70)"
        },
        {
            label: "Calibration max lap count",
            name: "calib_max_lap_count",
            help: "The number of laps to be detected to finish the calibration."
        },
        {
            label: "Calibration min rssi peak",
            name: "calib_min_rssi_peak",
            help: "The minimum RSSI value to detect a 'drone enter gate' during" +
                " calibration."
        }].forEach((l) => {
            this.addElement(new RSSIConfigElement(cfg, idx,l.name, l.label, l.help));
        });
    }
}

export class TabedRssiConfigGroup extends ConfigGroup {
    tabs: Array<RSSITap>;
    tabList: HTMLUListElement;
    tabContent: HTMLDivElement;
    plusTabListElement: HTMLLIElement;
    minusTabListElement: HTMLLIElement;
    min_tabs: number;
    max_tabs: number;

    public drawTab(e: RSSITap){
        this.plusTabListElement.insertAdjacentElement("beforebegin", e.getTabLink());
        this.tabContent.appendChild(e.getTabPane());
    }

    private updatePlusMinus() {
        if (this.max_tabs - this.min_tabs == 0) {
            hide(this.tabList);
        }
        if (this.tabs.length >= this.max_tabs) {
            hide(this.plusTabListElement);
        } else {
            show(this.plusTabListElement)
        }

        if (this.tabs.length <= this.min_tabs) {
            hide(this.minusTabListElement);
        } else {
            show(this.minusTabListElement)
        }
    }

    public addTab(){
        if (this.tabs.length < this.max_tabs) {
            const index = this.tabs.length;
            const e = new RSSITap(this.cfg, index);
            if (index > 0 && e.getValue('freq') == "0") {
                e.initFromOther(this.tabs[index-1]);
            }
            this.tabs.push(e);
            this.drawTab(e);
            e.selectTab();
        } else {
            this.tabs[this.tabs.length -1].selectTab();
        }
        this.updatePlusMinus();
    }

    public removeTab() {
        if (this.min_tabs < this.tabs.length) {
            const e = this.tabs.pop();
            e.getTabPane().remove();
            e.getTabLink().remove();
        }
        this.tabs[this.tabs.length -1].selectTab();
        this.updatePlusMinus();
    }

    public buildTabElements() {
        this.tabList = ul({ class: 'nav nav-tabs' });
        this.tabContent = div({ class: 'tab-content' });

        this.plusTabListElement = li({ class: 'nav-item' }, [
                a({
                    class: `nav-link`,
                    'data-bs-toggle': 'tab',
                    onclick: () => {
                        this.addTab();
                    }
                }, "+"),
            ]);
        this.minusTabListElement = li({ class: 'nav-item' }, [
                a({
                    class: `nav-link`,
                    'data-bs-toggle': 'tab',
                    onclick: () => {
                        this.removeTab();
                    }
                }, "-"),
            ]);

        this.tabList.appendChild(this.plusTabListElement);
        this.tabList.appendChild(this.minusTabListElement);
    }

    public draw(): HTMLElement {

        setTimeout(()=> {
            this.tabs[0].selectTab();

        }, 500);
        return li({class:"list-group-item bg-gradient rounded", style: "margin-bottom: 5px; padding: 5px"},
            h5({style: "text-align: right; margin:0"}, this.label),
            div(this.tabList, this.tabContent)
        );
    }

   public getKeyValues(obj_r?: Object): Object{
        var obj = new Object();
        for(const e of this.tabs){
            for (const [key, value] of Object.entries(e.getKeyValues(obj_r)) ){
                obj[key] = value;
            }
        }
        for (let i = this.tabs.length; i < 8; i++) {
            obj_r[`rssi[${i}].freq`] = "0";
            obj[`rssi[${i}].freq`] = "0";
        }
        return obj;
    }


    constructor(cfg: Config, label: string, min_tabs: number, max_tabs: number) {
        super(cfg, label);
        this.tabs = new Array<RSSITap>();
        this.min_tabs = min_tabs;
        this.max_tabs = max_tabs;

        this.buildTabElements();
        for(const e of cfg.rssi) {
            if (e.freq != 0)
                this.addTab();
            else
                break;
        }

        while(cfg.rssi.length < min_tabs)
            this.addTab();
    }
}



export class ElrsConfigGroup extends ConfigGroup {
   constructor(cfg:Config) {
        super(cfg, "expressLRS/OSD");

        this.addElement(new ConfigElement(cfg, "elrs_uid", "Elrs UID"));
        this.addElement(new ConfigElement(cfg, "osd_x", "X position on OSD"));
        this.addElement(new ConfigElement(cfg, "osd_y", "Y position on OSD"));
        this.addElement(new ConfigElement(cfg, "osd_format", "OSD message format"));
        // TODO test OSD element
    }
}

export class WifiConfigGroup extends ConfigGroup {
   constructor(cfg: Config) {
        super(cfg, "WiFi Settings");

        this.addElement(new ConfigSelectElement(cfg, "wifi_mode", "WIFI Mode",
            enumToMap(ConfigWIFIMode)));
        this.addElement(new ConfigElement(cfg, "ssid", "SSID"));
        this.addElement(new ConfigElement(cfg, "passphrase", "Passphrase"));
    }
}


export class RaceConfigPage extends Page {
    root: HTMLElement;

    drawConfig(cfg: Config) : HTMLElement {
        var groups = new Array<ConfigGroup>();

        groups.push(new GeneralConfigGroup(cfg));
        groups.push(new TabedRssiConfigGroup(cfg, "Player Config", 1, 1));
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
        super("Config");

    }
}
