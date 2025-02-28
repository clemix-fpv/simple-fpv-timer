import van from "../../lib/van-1.5.2.js"
import { Notifications } from "../../Notifications.js";
import { Config, ConfigGameMode, ConfigNodeMode, ConfigWIFIMode, Lap, Page, Player } from "../../SimpleFpvTimer.js";
import { $, enumToMap, format_ms, hide, Images, numberToColor, show, suffix } from "../../utils.js";

const { h3,label, form, select,input,img,fieldset, option, button, div, h5, pre, ul, li, span, a, table, thead, tbody, th, tr,td} = van.tags


export class VisibleElements {
    protected hidden: Array<string>;
    protected hidden_groups: Array<string>;
    protected rssi_min: number;
    protected rssi_max: number;
    protected rssi_label: string;

    public fieldIsVisible(fieldname: string): boolean {
        return !this.inArray(this.hidden, fieldname);
    }

    public groupIsVisible(group: string): boolean {
        return !this.inArray(this.hidden_groups, group);
    }

    private inArray(arr: Array<string>, needle: string) {
        return !!arr.find((v) => {
            if (v.charAt(0) == '^') {
                return !! needle.match(v);
            } else {
                return v === needle;
            }
        });
    }

    get getRssiMin() {
        return this.rssi_min;
    }

    get getRssiMax() {
        return this.rssi_max;
    }

    get getRssiLabel() {
        return this.rssi_label;
    }

    constructor(
        cfg: Config,
        hidden?: Array<string>,
        hidden_groups?: Array<string>,
        rssi_min?:number,
        rssi_max?: number) {

        this.init(cfg, hidden, hidden_groups, rssi_min, rssi_max);
    }

    init(
        _cfg: Config,
        hidden?: Array<string>,
        hidden_groups?: Array<string>,
        rssi_min?:number,
        rssi_max?: number) {

        if (!hidden) {
            hidden = new Array<string>();
        }

        if (!hidden_groups)
            hidden_groups = new Array<string>();

        this.hidden = hidden;
        this.hidden_groups = hidden_groups;
        this.rssi_min = rssi_min || 1;
        this.rssi_max = rssi_max || 8;
    }
}

export class ElementsRace extends VisibleElements {

    init(
        cfg: Config,
        hidden?: Array<string>,
        hidden_groups?: Array<string>,
        _rssi_min?:number,
        _rssi_max?: number) {
        super.init(cfg, hidden, hidden_groups, 1, 1);

        this.rssi_label = "Player Settings";
        this.hidden.push('rssi[0].led_color');
        if (cfg.node_mode == ConfigNodeMode.CONTROLLER) {
            this.hidden.push('ctrl_ipv4');
            this.hidden.push('node_name');
        }
    }
}

class ElementsCTF extends VisibleElements {
    init(
        cfg: Config,
        hidden?: Array<string>,
        hidden_groups?: Array<string>,
        rssi_min?:number,
        rssi_max?: number) {
        super.init(cfg, hidden, hidden_groups, rssi_min, rssi_max);

        this.rssi_label = "CTF Teams";
        this.hidden_groups.push(ElrsConfigGroup.name)

        if (cfg.node_mode == ConfigNodeMode.CONTROLLER)
            this.hidden.push('ctrl_ipv4');
        else
            this.hidden_groups.push(TabedRssiConfigGroup.name)
    }
}

class ElementsSpectrum extends VisibleElements {
    init(
        cfg: Config,
        hidden?: Array<string>,
        hidden_groups?: Array<string>,
        rssi_min?:number,
        rssi_max?: number) {
        super.init(cfg, hidden, hidden_groups, rssi_min, rssi_max);
        this.hidden_groups.push(ElrsConfigGroup.name)

        this.hidden.push('ctrl_ipv4');
        this.hidden.push('node_name');
        this.hidden.push('node_mode');
        this.hidden.push('^rssi\\[\\d+\\]\\.(?!freq)');
    }
}

export class ConfigElement extends EventTarget {
    fieldname: string;
    value: string;
    label: string;
    help: string;
    cfg: Config;
    visible: boolean;

    constructor(cfg: Config, visible: VisibleElements, fieldname: string, label: string, help?: string, value?: string) {
        super();
        this.visible = visible.fieldIsVisible(fieldname);
        this.fieldname = fieldname;
        this.value = value ? value : cfg.getValue(fieldname);
        this.label = label;
        if (help)
            this.help = help;
    }

    inputId() {
        return `id_input_${this.fieldname}`;
    }

    public draw() {
        return this.visible ? this.buildHtmlElement() : div();
    }

    public buildHtmlElement() {
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
        return String(this.value);
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

export class ConfigPasswordElement extends ConfigElement {
    public buildHtmlElement() {
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
                    id: this.inputId(), type: "password", value: this.value})
            )
        );
    }
}


export class ConfigColorElement extends ConfigElement {
    public buildHtmlElement() {
        var value = "#000000";
        var v = this.value || 0;

        if (!Number.isNaN(v))
            value = numberToColor(v as number);

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

    constructor(cfg: Config, visible: VisibleElements,
        fieldname: string, label: string, options: Map<string, string>, help?: string) {
        super(cfg, visible, fieldname, label, help);
        this.options = options;
    }


    public buildHtmlElement() {

        var s = select({class: "form-select", name: this.fieldname, id: this.inputId(),
            onchange: () => {
                var i = $(this.inputId()) as HTMLSelectElement;
                this.dispatchEvent(new CustomEvent('onchange', { detail: { element: this, value: i.value } }));
            }
        });
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
    visible: boolean;

    constructor(cfg: Config, visible: VisibleElements, label: string) {
        this.label = label;
        this.cfg = cfg;
        this.elements = new Array<ConfigElement>();
        this.visible = visible.groupIsVisible(this.constructor.name);
    }

    addElement(elem: ConfigElement) : ConfigGroup{
        this.elements.push(elem);
        return this;
    }

    public buildHtmlElement(): HTMLElement {
        return li({class:"list-group-item config-group rounded bg-gradient", },
            h5({style: "text-align: right; margin:0"}, this.label),
            this.elements.map((e) => {
                return e.draw();
            })
        );
    }

    public draw() : HTMLElement {
        return this.visible ? this.buildHtmlElement() : div();
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

    public findConfigElementById(id: string): ConfigElement {
        for (const elem  of this.elements) {
            if (elem.fieldname == id) {
                return elem;
            }
        }
        return null;
    }

}

class RSSIConfigElement extends ConfigElement {
    constructor(cfg: Config, visible: VisibleElements, idx: number, fieldname: string, label: string, help?: string) {
        super(cfg, visible, `rssi[${idx}].${fieldname}`, label, help);
    }
}

class IPSocketElement extends ConfigElement {
    port: string;
    ip: string;

    constructor(cfg: Config, visible: VisibleElements, fieldname_ip: string, fieldname_port: string, label: string, help?: string) {
        super(cfg,
            visible,
            fieldname_ip,
            label,
            help,
            cfg.getValue(fieldname_ip) + ":" + cfg.getValue(fieldname_port));
        this.port = fieldname_port;
        this.ip = fieldname_ip;
    }

    public getKeyValue(obj_r?: Object): Object{
        var obj = new Object();
        var v = this.getValue();
        if (v) {
            var splits = v.split(":");
            obj[this.ip] = splits[0];
            if (splits.length > 1)
                obj[this.port] = splits[1];
            if (obj_r)
                obj_r[this.ip] = splits[0];
                if (splits.length > 1)
                    obj[this.port] = splits[1];
            return obj;
        }
        return obj;
    }

}

export class GeneralConfigGroup extends ConfigGroup {
    constructor(cfg: Config, visible: VisibleElements) {
        super(cfg, visible, "General settings");
        var game_mode = new ConfigSelectElement(cfg, visible,
            "game_mode", "Game mode", enumToMap(ConfigGameMode))
        var node_name = new ConfigElement(cfg, visible, "node_name", "Device Name",
            "Used for CTF, could descripe the location of this device");

        this.addElement(game_mode);
        this.addElement(node_name);

        var node_mode = new ConfigSelectElement(cfg, visible, "node_mode", "Device Mode",
            enumToMap(ConfigNodeMode),
            "Specify if this is a controller device or a client which connects to a controller."
        );
        var ctrl_ipv4 = new IPSocketElement(cfg, visible, "ctrl_ipv4", "ctrl_port", "Controller IPv4",
            "The controller IPv4 address to connect to. If value is 0.0.0.0, GW is used.");

        this.addElement(node_mode);
        this.addElement(ctrl_ipv4);

        this.addElement(new ConfigElement(cfg, visible, "led_num", "Number of LEDs"));
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

    public buildHtmlElement() : HTMLElement {
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
            'calib_min_rssi_peak',
            'led_color'
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

    public setValue(fieldname: string, value: string) {
        var e = this.findConfigElementById(`rssi[${this.index}].${fieldname}`);
        if (e) {
            e.setValue(value);
        }
    }

    constructor(cfg: Config, visible: VisibleElements, idx: number) {
        super(cfg, visible, "Player settings");
        this.index = idx;
        this.label = (idx + 1).toString();


        this.addElement(new RSSIConfigElement(cfg, visible, idx, "name", "Name"));
        this.addElement(new ConfigSelectElement(cfg, visible, `rssi[${idx}].freq`,
            "Frequency", freq_map));

        this.addElement(new ConfigColorElement(cfg, visible, `rssi[${idx}].led_color`, "LED color"));

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
            this.addElement(new RSSIConfigElement(cfg, visible, idx,l.name, l.label, l.help));
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
    visible_elements: VisibleElements;

    public drawTab(e: RSSITap){
        this.plusTabListElement.insertAdjacentElement("beforebegin", e.getTabLink());
        this.tabContent.appendChild(e.getTabPane());
    }

    private updatePlusMinus() {
        if (this.max_tabs - this.min_tabs == 0) {
            hide(this.tabList);
            return;
        }

        show(this.tabList);
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
            const new_tab = new RSSITap(this.cfg, this.visible_elements, index);
            if (index > 0 && new_tab.getValue('freq') == "0") {
                new_tab.initFromOther(this.tabs[index-1]);
                for (const freq of freq_map.keys()) {
                    if (!this.tabs.find((e) => e.getValue("freq") == freq)) {
                        new_tab.setValue("freq", freq);
                        break;
                    }
                }
            }
            this.tabs.push(new_tab);
            this.drawTab(new_tab);
            new_tab.selectTab();
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

    public buildHtmlElement(): HTMLElement {
        return li({class:"list-group-item config-group rounded bg-gradient"},
            h5({style: "text-align: right; margin:0"}, this.label),
            div(this.tabList, this.tabContent)
        );
    }

    public draw(): HTMLElement {
        setTimeout(()=> {
            if (this.tabs.length > 0)
                this.tabs[0].selectTab();
        }, 500);
        return this.visible ? this.buildHtmlElement(): div();
    }

   public getKeyValues(obj_r?: Object): Object{
        var obj = new Object();
        for(const e of this.tabs){
            for (const [key, value] of Object.entries(e.getKeyValues(obj_r)) ){
                obj[key] = value;
            }
        }
        for (let i = this.tabs.length; i < 8; i++) {
            if (obj_r)
                obj_r[`rssi[${i}].freq`] = "0";
            obj[`rssi[${i}].freq`] = "0";
        }
        return obj;
    }

    public findConfigElementById(id: string): ConfigElement {
        var e: ConfigElement;

        for(const tab of this.tabs){
            if (e = tab.findConfigElementById((id)))
                return e;
        }
        return null;
    }

    constructor(cfg: Config, visible: VisibleElements) {
        super(cfg, visible, visible.getRssiLabel);
        this.visible_elements = visible;
        this.tabs = new Array<RSSITap>();
        this.min_tabs = visible.getRssiMin;
        this.max_tabs = visible.getRssiMax;

        if (this.visible) {
            this.buildTabElements();

            for(const rssi of cfg.rssi) {
                if (rssi.freq == 0 || this.tabs.length >= this.max_tabs)
                    break;
                this.addTab();
            }
            while(this.tabs.length < this.min_tabs)
                this.addTab();
        }
    }
}


export class ElrsConfigGroup extends ConfigGroup {
   constructor(cfg:Config, visible: VisibleElements) {
        super(cfg, visible, "expressLRS/OSD");

        this.addElement(new ConfigElement(cfg, visible, "elrs_uid", "Elrs UID"));
        this.addElement(new ConfigElement(cfg, visible, "osd_x", "X position on OSD"));
        this.addElement(new ConfigElement(cfg, visible, "osd_y", "Y position on OSD"));
        this.addElement(new ConfigElement(cfg, visible, "osd_format", "OSD message format"));
        // TODO test OSD element
    }
}

export class WifiConfigGroup extends ConfigGroup {
   constructor(cfg: Config, visible: VisibleElements) {
        super(cfg, visible, "WiFi Settings");

        this.addElement(new ConfigSelectElement(cfg, visible, "wifi_mode", "WIFI Mode",
            enumToMap(ConfigWIFIMode)));
        this.addElement(new ConfigElement(cfg, visible, "ssid", "SSID"));
        this.addElement(new ConfigPasswordElement(cfg, visible, "passphrase", "Passphrase"));
    }
}

export class ConfigForm {
    groups: Array<ConfigGroup>;
    root: HTMLElement;
    visible: VisibleElements;

    private findConfigElementById(id: string): ConfigElement {
        for (const group of this.groups) {
            var elem = group.findConfigElementById(id);
            if (elem)
                return elem;
        }
        return null;
    }

    private visibleByGameMode(cfg: Config) {
        switch (Number(cfg.game_mode)) {
            case ConfigGameMode.RACE:
                return new ElementsRace(cfg);
            case ConfigGameMode.CTF:
                return new ElementsCTF(cfg);
            case ConfigGameMode.SPECTRUM:
                return new ElementsSpectrum(cfg);
            default:
                console.error("Unhandled game_mode: " + cfg.game_mode);
        }
        return null;
    }

    public draw(): HTMLElement{
        return this.root;
    }

    private submitButton() {
            return div({class: "mb-3", style: "margin: 5px 3px"},
                    button({class: "btn btn-primary", type: "button", id: "config_submit_btn_id", onclick: () => {
                        var btn = $('config_submit_btn_id') as HTMLButtonElement;
                        btn.disabled = true;
                        btn.classList.add("disabled");

                        var cfg = new Config();
                        cfg.setValues(this.getKeyValues());

                        cfg.save((cfg: Config) => {
                            if (cfg)
                                this.updateForm(cfg);

                            btn.classList.remove("disabled");
                            btn.disabled = false;
                        });
                    }}, "Submit")
                );
    }

    private updateRoot() {
        this.root.replaceChildren(
            form(
                this.groups.map((g) => {
                    return g.draw();
                })
                ,
                this.submitButton()
            )
        );
    }

    public getKeyValues(obj_r?: Object): Object{
        var obj = new Object();
        for(const group of this.groups){
            for (const [key, value] of Object.entries(group.getKeyValues(obj_r)) ){
                obj[key] = value;
            }
        }
        return obj;
    }

    updateForm(cfg: Config) {
        this.groups = new Array<ConfigGroup>();
        if (!this.visible) {
            this.visible = this.visibleByGameMode(cfg);
        } else {
            this.visible.init(cfg);
        }
        this.groups.push(new GeneralConfigGroup(cfg, this.visible));
        this.groups.push(new TabedRssiConfigGroup(cfg, this.visible));
        this.groups.push(new ElrsConfigGroup(cfg, this.visible));
        this.groups.push(new WifiConfigGroup(cfg, this.visible));

        var game_mode = this.findConfigElementById("game_mode");
        if (game_mode) {
            game_mode.addEventListener("onchange", () => {
                var cfg = new Config();
                cfg.setValues(this.getKeyValues());
                this.updateForm(cfg);
            });
        }
        var node_mode = this.findConfigElementById("node_mode");
        if (node_mode) {
            node_mode.addEventListener("onchange", () => {
                var cfg = new Config();
                cfg.setValues(this.getKeyValues());
                this.updateForm(cfg);
            });
        }
        this.updateRoot();
    }

    constructor(cfg: Config, visible?: VisibleElements) {
        this.root = div();
        if (visible)
            this.visible = visible;
        this.updateForm(cfg);
    }
}


export class RaceConfigPage extends Page {
    root: HTMLElement;

    getDom(): HTMLElement {
        if (! this.root) {
            this.root = div();
        }

        var cfg = new Config();
        cfg.update((cfg: Config) => {
            this.root.replaceChildren(new ConfigForm(cfg).draw());
        });
        return this.root;
    }

    constructor() {
        super("Config");

    }
}
