
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


