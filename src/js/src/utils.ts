import { Notifications } from "./Notifications";

export function format_ms_short(arg: number|string) {

    var sec_num = typeof arg === "string" ? parseInt(arg, 10): arg;
    var hours   = Math.floor(sec_num / 3600 / 1000);
    var minutes = Math.floor((sec_num - (hours * 3600 * 1000)) / 60 / 1000);
    var seconds = Math.floor((sec_num - (hours * 3600 * 1000)  - (minutes * 60 * 1000)) / 1000);
    var ms = sec_num - (hours * 3600 * 1000) - (minutes * 60 * 1000) - seconds * 1000;

    var t = [[hours, "h"], [minutes,"m"], [seconds,"."], [ms,"s"]];
    var ret = "";
    for (var i=0; i < t.length; i++){
        ret += "" + t[i][0] + t[i][1];
    }
    return ret;
}


export function format_ms(arg: number|string) {
    var sec_num = typeof arg === "string" ? parseInt(arg, 10): arg;
    var hours   = Math.floor(sec_num / 3600 / 1000);
    var minutes = Math.floor((sec_num - (hours * 3600 * 1000)) / 60 / 1000);
    var seconds = Math.floor((sec_num - (hours * 3600 * 1000)  - (minutes * 60 * 1000)) / 1000);
    var ms = sec_num - (hours * 3600 * 1000) - (minutes * 60 * 1000) - seconds * 1000;

    var t = [[hours.toString(), "h"], [minutes.toString(),"m"], [seconds.toString(),"s"], [ms.toString(),"ms"]];
    var p = false;
    var ret = "";
    for (var i=0; i < t.length; i++){
        if (t[i][0] != "0" || p) {
            ret += t[i][0] + t[i][1] + " ";
            p=true;
        }
    }
    return ret;
}

export function $(element : string) {
    return document.getElementById(element);
}

export function hide(element: HTMLElement) {
    if (element.style.display !== "none") {
        element["old_display"] = element.style.display;
        element.style.display = "none";
    }
}

export function show(element: HTMLElement) {
    if (element.style.display === "none")
        element.style.display = element["old_display"] || "initial";
}

export function enumToMap(obj: Object) {
    var m = new Map<string, string>();

    for( const key  of Object.keys(obj)) {
        if (!Number.isNaN(Number(key))) {
            m.set(key, obj[key]);
        }
    }
    return m;
}

export function suffix(v: String) {
    return v.slice((v.lastIndexOf(".") - 1 >>> 0) + 2);
}

export class Images {
    public static get QUESTION_SVG():string { return "question-circle.svg"; }
}


export function nullOrUndef(variable: any, default_val: string): any | string {
    if (typeof variable === 'undefined' || variable === null) {
        return default_val;
    }
    return variable;
}

export function toColor(value: number | string): string {
    var v =  value || 0;
    const regex = /^#[0-9a-fA-F]{6}$/;

    if (regex.test(v as string))
        return v as string;

    if (!Number.isNaN(v))
        return numberToColor(v as number);

    return "#000000";
}

export function numberToColor(v: number): string {
    v = v as number;
    var r = (v & 0xff0000) >> 16;
    var g = (v & 0x00ff00) >> 8;
    var b = (v & 0x0000ff);
    return "#" + r.toString(16).padStart(2, '0') +
        g.toString(16).padStart(2,'0') + b.toString(16).padStart(2,'0');
}

/**
 * on_success is a the callback function, which is called once the request was successful.
 * The retrieved json is passed as first argument to this function.
 *
 * on_error is called when en error happen, and get's the full response object.
 */
export async function getJSON(url: string,
    on_success: CallableFunction,
    on_error?: CallableFunction) {

        const response = await fetch(url);
        if (!response.ok) {
            if (on_error) {
                on_error(response);
            } else {
                Notifications.showError({msg: `Failed to request JSON from ${url} status:${response.status}`});
            }
        } else {
            const json = await response.json();
            on_success(json);
        }
}


