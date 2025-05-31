import van from "../../lib/van-1.5.2.js"
import { Lap, Page, Player, SimpleFpvTimer } from "../../SimpleFpvTimer";
import { format_ms } from "../../utils.js";
const {button, div, pre, ul, li, a, table, thead, tbody, th, tr,td} = van.tags

class LapsRow {
    player: Player;
    lap: Lap;

    public draw(style?: {class?: string}) {
        return tr(
            style,
            td(this.player.name),
            td(this.lap.id),
            td(format_ms(this.lap.duration)),
            td(this.lap.rssi),
        );
    }

    constructor(player: Player, lap: Lap) {
        this.player = player;
        this.lap = lap;
    }
}

class LapsTable {
    headline: string[];
    laps: LapsRow[];

    public addRow(row: LapsRow) {
        this.laps.push(row);
    }

    public sortByDuration() {
        this.laps.sort((a,b) => {
            return a.lap.duration - b.lap.duration;
        });
    }

    public sortByAbsTime() {
        this.laps.sort((a,b) => {
            return b.lap.abs_time - a.lap.abs_time;
        });
    }

    public draw() : HTMLElement{

        this.sortByDuration();
        var first = this.laps[0];
        var second = this.laps[1];
        var third = this.laps[2];

        const r = tr({class: 'table-secondary'});
        this.headline.forEach(e => {
           r.append(th({scope: "col"}, e));
        });

        const head = thead(r);

        const body = tbody();
        this.laps.forEach((l: LapsRow) => {
            var r = l.draw();
            if ( l == first) {
                r.getElementsByTagName("td").item(2).innerHTML += "&#129351;";
            } else if (l == second) {
                r.getElementsByTagName("td").item(2).innerHTML += "&#129352;";
            } else if (l == third) {
                r.getElementsByTagName("td").item(2).innerHTML += "&#129353;";
            }

            body.append(r);
        });

        const t = table(
            {class: 'table table-striped table-sm table-dark' },
            head, body);

        return t;
    }

    constructor() {
        this.headline = ["Pilot", "Lap#", "Duration", "RSSI"];
        this.laps = new Array<LapsRow>();
    }
}

export class LapsPage extends Page {
    _root: HTMLElement;
    lapsTable: LapsTable;

    private get root() {
        if (! this._root) {
            this._root = div(this.lapsTable.draw());
        }
        return this._root;
    }

    getDom(): HTMLElement {
        SimpleFpvTimer.requestPlayersUpdate();
        return this.root;
    }

    onPlayersUpdate(players: Player[]) {
        this.lapsTable = new LapsTable();

        players.forEach((player: Player) => {
            player.laps.forEach((lap: Lap) => {
                this.lapsTable.addRow(new LapsRow(player, lap));
            });
        });

        this.lapsTable.sortByAbsTime();

        this.root.replaceChildren(this.lapsTable.draw());
    }

    constructor() {
        super("Laps");
        this.lapsTable = new LapsTable();
        document.addEventListener("SFT_PLAYERS_UPDATE", (e: CustomEventInit<Player[]>) => {
            this.onPlayersUpdate(e.detail);
        })
    }

}
