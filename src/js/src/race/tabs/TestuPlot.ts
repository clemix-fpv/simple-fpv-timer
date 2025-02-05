import uPlot, { Options, Axis, AlignedData } from "../../lib/uPlot.js";
import van from "../../lib/van-1.5.2.js"
import { Page } from "../../SimpleFpvTimer";

const {button, div, pre} = van.tags


export class TestuPlot extends Page {


    getData() {
        var d = [
            [
                [0, 0],
                [700, 700],
            ],
            [
                [0, 1, 2, 3 ,4 ],
                [ 100, 200, 900, 400, 800],
            ]
        ];

        return uPlot.join(d);
    }

    getData2() {
        var d = [
                [0, 1, 2, 3 ,4 ],
                [ 100, 100, 100, 100, 100],
                [ 100, 200, 900, 400, 800],
        ];

        return d;
    }



    getDom(): HTMLElement {
        const opts: Options = {
            title: "",
            width: 1048,
            height: 600,
            scales: {
                x: {
                    time: false,
                },
                y: {
                    auto: false,
                    range: [0, 1300],
                },
            },
            series: [
                {
                    label: "time:",
                },
            {
                stroke: "#70dba4",
                label: "peak",
            },
                {
                    stroke: "#982463",
                    label: "freq",
                }
            ],
            axes: [
                {
                    label: "X Axis Label",
                    labelSize: 20,
                },
                {
                    space: 50,
                    side: 1,
                    label: "RSSI",
                    labelGap: 8,
                    labelSize: 8 + 12 + 8,
                    stroke: "grey",
                }
            ],
        };

        var d = div();
        new uPlot(opts, this.getData2() , d);
        return d;
    }

    constructor() {
        super("uPlot");
    }
}
