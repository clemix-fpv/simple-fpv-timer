import van from "../../lib/van-1.5.2.js"
import { Notifications } from "../../Notifications.js";
import { Lap, Page, Player } from "../../SimpleFpvTimer.js";
import { $, enumToMap, format_ms } from "../../utils.js";

const { h3,label, select, option, button, div, h5, input, pre, ul, li, span, a, table, thead, tbody, th, tr,td} = van.tags

enum RaceMode {
    OneLap,
    TwoLaps,
    ThreeLaps,
    MaxLaps,
    FastesLap,
    FastesTwoLaps,
    FastesThreeLaps,
    FastesTwoContinousLaps,
    FastesThreeContinousLaps,
};

function numberToRaceMode(v: number) : RaceMode {
 if (v == RaceMode.FastesTwoContinousLaps) {
 return RaceMode.FastesTwoContinousLaps;
 }

    throw Error;
}


class PlayerRanking {
    player: Player;
    ranking: number;
    counted_laps: Lap[];

    constructor(player: Player, ranking: number) {
        this.player = player;
        this.ranking = ranking;
        this.counted_laps = new Array<Lap>();
    }

    public compare(other: PlayerRanking) {
        return other.ranking - this.ranking;
    }

    private drawLap(id: string, time: string, style?:string) {
        return div({class: "card-text border rounded ", style: `margin: 3px; ${style}`},
            span({style: "font-weight: bold; border-right: 1px solid grey; margin: 0px 5px; min-width: 40px; display: inline-block;"}, id),
            span(time)
        );
    }

    public drawLapInfo() : HTMLElement{
        var laps = this.counted_laps.length > 0 ? this.counted_laps : this.player.laps;
        var sum_div = div();
        if (laps.length > 1) {
            var sum_duration = 0;
            for(const l of laps){
                sum_duration += l.duration;
            }
            sum_div = this.drawLap("SUM", format_ms(sum_duration), "font-weight: bold;");
        }
        return div(
            laps.map((lap: Lap) => {
                return  this.drawLap(lap.id.toString(), format_ms(lap.duration));
            }),
            sum_div
        );
    }
}

function sortPlayerRanking(a: PlayerRanking, b: PlayerRanking) {
    return a.ranking - b.ranking;
}

class PlayerRankingByFastesLap extends PlayerRanking {
    constructor(player: Player, laps: number) {
        var counted_laps = new Array<Lap>();

        player.sortLapsByDuration();

        var duration_sum = 0;

        if (player.laps.length >= laps) {
            player.laps.forEach((l:Lap, i:number) => {
                if (i < laps) {
                    duration_sum += l.duration;
                    counted_laps.push(l);
                }
            });
        } else {
            duration_sum = Number.MAX_VALUE;
        }

        super(player, duration_sum);
        this.counted_laps = counted_laps;
    }
}

class PlayerRankingByLap extends PlayerRanking {
    constructor(player: Player, laps: number) {
        var counted_laps = new Array<Lap>();
        player.sortLapsById();

        var duration_sum = 0;

        if (player.laps.length >= laps) {
            player.laps.forEach((l:Lap, i:number) => {
                if (i < laps) {
                    duration_sum += l.duration;
                    counted_laps.push(l);
                }
            });
        } else {
            duration_sum = Number.MAX_VALUE;
        }

        super(player, duration_sum);
        this.counted_laps = counted_laps;
    }
}

class PlayerRankingByContinousLaps extends PlayerRanking {
    constructor(player: Player, num: number) {
        var continuous_duration = new Array<{laps: Lap[], duration_sum: number}>;

        player.laps.forEach((l:Lap, i:number) => {
            var sum = 0;
            var laps = new Array<Lap>();
            if (i + num <= player.laps.length) {
                for (let j = 0; j < num; j++) {
                    sum += player.laps[i + j].duration;
                    laps.push(player.laps[i + j]);
                }
            } else {
                sum = Number.MAX_VALUE;
            }
            continuous_duration.push({laps: laps, duration_sum: sum})
        });

        continuous_duration.sort((a,b) =>
            { return a.duration_sum - b.duration_sum});

        super(player, continuous_duration.length > 0 ?
            continuous_duration[0].duration_sum : Number.MAX_VALUE);
        if (continuous_duration.length > 0)
            this.counted_laps = continuous_duration[0].laps;
    }
}

export class PlayersPage extends Page {
    root: HTMLElement;
    playersDom: HTMLElement;
    actionsDom: HTMLElement;
    players: PlayerRanking[];
    racemode: RaceMode;

    private async startRace() {
        const url = "/api/v1/clear_laps";
        const input_offset = $('input_start_race_offset') as HTMLInputElement;
        var offset = 30000;
        if (input_offset) {
            if (!Number.isNaN(Number(input_offset.value)))
                offset = Number(input_offset.value) * 1000;
        }
        const response = await fetch(url, {
            method: 'POST',
            headers: {
                    'Content-Type': 'application/json; charset=utf-8'
            },
            body: JSON.stringify({offset: offset})
        });
        if (!response.ok) {
            Notifications.showError({msg: `Failed to save config ${response.status}`})
        } else {
            document.dispatchEvent(
                new CustomEvent("SFT_PLAYERS_UPDATE", {detail: new Array<Player>()})
            );
            Notifications.showSuccess({msg: "Laps cleared, 🏁🏁🏁 RACE 🏁🏁🏁"});
        }
    }

    getActionsDom(): HTMLElement {
        if (!this.actionsDom) {
            this.actionsDom = div(
                div({class: "mb-3", style: "margin: 5px 3px"},
                    div({class: "input-group flex-nowrap"},
                        span( {class: "input-group-text", id: "inputGroup-sizing-default"},
                            "Race mode"),
                        select({class: "form-select", id: "select_race_mode",
                            onchange: () => {
                                var i = $("select_race_mode") as HTMLSelectElement;
                                this.racemode = Number(i.value) as RaceMode;
                                this.onPlayersUpdate(this.players.map((p) => {
                                    return p.player;
                                }));
                            }},
                            Array.from(enumToMap(RaceMode)).map(([key, value]) => {
                                    return option({value: key}, value.replace(/([A-Z])/g, ' $1'))
                                })
                        )),
                    div({class: "form-text"},
                        "Select race mode to set the evaluation of the laps.")
                ),

                div({class: "mb-3", style: "margin: 5px 3px"},
                    div({class: "input-group flex-nowrap"},
                        button({class: "btn btn-primary", type: "button", onclick: () => {
                            this.startRace();
                        }}, "Start Race"),
                    span({class: "input-group-text"},"in"),
                    input({class: "input-group-text", value: "30", id: "input_start_race_offset"}),
                    span({class: "input-group-text"},"sec")
                    )
                )
            );
        }
        return this.actionsDom;
    }

    getPlayersDom(): HTMLElement {
        if (!this.playersDom) {
            this.playersDom = div();
        }
        return this.playersDom;
    }

    getDom(): HTMLElement {
        if (! this.root) {
            this.root = div(
                this.getActionsDom(),
                this.getPlayersDom()
            );
        }
        return this.root;
    }

    drawPlayerRanking(p: PlayerRanking): HTMLElement {

        var medal = (p.ranking == 1)? "🏆" /* "&#127942;" || "&#129351;"*/ :
                    (p.ranking == 2)? "🥈" /*"&#129352;"*/ :
                    (p.ranking == 3)? "🥉":
                    (p.ranking == Number.MAX_VALUE) ? "" :
                    p.ranking + ".";

        var card = div({class:"card", style: "margin: 5px;"},
            h5({class: "card-header"}, medal + " " + p.player.name,
                a({class: "btn btn-secondary btn-sm",
                    style: "float:right", role: "button",
                    href: "http://" + p.player.ipaddr }, "⚙️" /*"&#9881;"*/)
                ),
            div({class: "card-text", style: "font-weight: bold;"}, "Laps:"),
            p.drawLapInfo()
        );
        return card;
    }

    sortPlayersByFastesLap(players: Player[], laps: number) {
        var ranking = new Array<PlayerRankingByFastesLap>();

        players.forEach((p:Player) => {
            ranking.push(new PlayerRankingByFastesLap(p, laps));
        });

        return ranking;
    }

    sortPlayersByLap(players: Player[], laps: number) {
        var ranking = new Array<PlayerRankingByLap>();

        players.forEach((p:Player) => {
            ranking.push(new PlayerRankingByLap(p, laps));
        });
        return ranking;
    }

    sortPlayersByMaxLap(players: Player[]) {
        var ranking = new Array<PlayerRankingByLap>();

        players.sort((a,b) => {
            if (a.laps.length == b.laps.length) {
                var sum_a = 0;
                var sum_b = 0;
                for (const l of a.laps) {
                    sum_a += l.duration;
                }
                for (const l of b.laps) {
                    sum_b += l.duration;
                }

                return sum_a - sum_b;
            }
            return b.laps.length - a.laps.length;
        });

        players.forEach((p:Player, i:number) => {
            ranking.push(new PlayerRanking(p, i));
        });

        return ranking;
    }

    sortPlayersByContinousLaps(players: Player[], num: number) {
        var ranking = new Array<PlayerRankingByLap>();

        players.forEach((p:Player) => {
            ranking.push(new PlayerRankingByContinousLaps(p, num));
        });
        return ranking;
    }

    public sortPlayersByMode(players: Player[], mode: RaceMode) : PlayerRanking[] {

        switch (mode) {
            case RaceMode.FastesLap:
                return this.sortPlayersByFastesLap(players, 1);
            case RaceMode.FastesTwoLaps:
                return this.sortPlayersByFastesLap(players, 2);
            case RaceMode.FastesThreeLaps:
                return this.sortPlayersByFastesLap(players, 3);

            case RaceMode.OneLap:
                return this.sortPlayersByLap(players, 1);
            case RaceMode.TwoLaps:
                return this.sortPlayersByLap(players, 2);
            case RaceMode.ThreeLaps:
                return this.sortPlayersByLap(players, 3);
            case RaceMode.MaxLaps:
                return this.sortPlayersByMaxLap(players);
            case RaceMode.FastesTwoContinousLaps:
                return this.sortPlayersByContinousLaps(players, 2);
            case RaceMode.FastesThreeContinousLaps:
                return this.sortPlayersByContinousLaps(players, 3);
            default:
                return this.sortPlayersByFastesLap(players, 1);
        }

    }

    public playersToRanking(players: Player[], mode: RaceMode): PlayerRanking[] {
        var ranking = this.sortPlayersByMode(players, mode);

        ranking.sort(sortPlayerRanking);

        var rank = 1;
        for(const r of ranking) {
            if (r.ranking != Number.MAX_VALUE)
                r.ranking = rank;
            rank += 1;
        }

        ranking.sort((a,b) => { return a.player.name.localeCompare(b.player.name)});

        return ranking;
    }

    onPlayersUpdate(players: Player[]) {

        this.players = this.playersToRanking(players, this.racemode)

        var container = div();
        this.players.forEach((p:PlayerRanking) => {
            container.appendChild(this.drawPlayerRanking(p));
        });
        this.getPlayersDom().replaceChildren(container);
    }

    constructor() {
        super("Players");
        this.racemode = RaceMode.FastesLap;
        this.players = new Array<PlayerRanking>();

        document.addEventListener("SFT_PLAYERS_UPDATE", (e: CustomEventInit<Player[]>) => {
            if (e.detail)
                this.onPlayersUpdate(e.detail);
        })
    }
}
