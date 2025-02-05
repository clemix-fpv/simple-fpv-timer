import { SimpleFpvTimer, Mode, Page, RssiEvent, ConfigGameMode } from "./SimpleFpvTimer.js"
import van from "./lib/van-1.5.2.js"
import "./lib/bootstrap.bundle.js"
import { RaceMode } from "./race/RaceMode.js"
import { Notifications } from "./Notifications.js"
import { TimeSync } from "./TimeSync.js"
import { CaptureTheFlagMode } from "./ctf/CaptureTheFlagMode.js"
import { TestuPlot } from "./race/tabs/TestuPlot.js"
import { SpectrumMode } from "./spectrum/SpectrumMode.js"

const {button, div, pre} = van.tags


const sleep = ms => new Promise(resolve => setTimeout(resolve, ms))

const Run = ({sleepMs}) => {
    const steps = van.state(0);

    (
        async () => {
        for (; steps.val < 40; ++steps.val) await sleep(sleepMs)
    }
    )(

    )

    return pre(() =>
        `${" ".repeat(40 - steps.val)}🚐💨Hello VanJS!${"_".repeat(steps.val)}`)
}

const Hello = () => {
    const dom = div()
    return div(
        dom,
        button({onclick: () => van.add(dom, Run({sleepMs: 2000}))}, "Hello 🐌"),
        button({onclick: () => van.add(dom, Run({sleepMs: 500}))}, "Hello 🐢"),
        button({onclick: () => van.add(dom, Run({sleepMs: 100}))}, "Hello 🚶‍♂️"),
        button({onclick: () => van.add(dom, Run({sleepMs: 10}))}, "Hello 🏎️"),
        button({onclick: () => van.add(dom, Run({sleepMs: 2}))}, "Hello 🚀"),
    )
}

class DebugPage extends Page {
    root: HTMLElement;

    getDom(): HTMLElement {
        if (!this.root) {
            this.root = div();
        }
        return this.root;
    }

    onRssiUpdate(ev: RssiEvent) {
        this.root.append(div(JSON.stringify(ev)));
    }

    constructor() {
        super("DEBUG");
        this.getDom();

        document.addEventListener("SFT_RSSI", (e: CustomEventInit<RssiEvent>) => {
            this.onRssiUpdate(e.detail);
        })
    }
}



const raceMode = new RaceMode ("Race");
raceMode.addPage(new DebugPage());

const ctfMode = new CaptureTheFlagMode("CTF");
ctfMode.addPage(new DebugPage());

const spectrumMode = new SpectrumMode("CTF");
spectrumMode.addPage(new DebugPage());

const app = new SimpleFpvTimer();
app.addMode(ConfigGameMode.RACE, raceMode);
app.addMode(ConfigGameMode.CTF, ctfMode);
app.addMode(ConfigGameMode.SPECTRUM, spectrumMode);

van.add(document.body, app.getDom());
const notifications = new Notifications();

TimeSync.instance().sync_time(null);
setTimeout(() => {
    console.debug("TimeSync offset: " + TimeSync.getOffset());
    }, 2000);

