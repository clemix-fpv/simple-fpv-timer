import { Notifications } from "./Notifications";

interface TimeSyncData {
    client: Array<number>;
    server: Array<number>;
}

export class TimeSync {
    static #instance: TimeSync;
    offset: number;

    constructor() {

    }

    static instance() : TimeSync {
        if (!TimeSync.#instance) {
            TimeSync.#instance = new TimeSync();
        }

        return TimeSync.#instance;
    }

    static getOffset() {
        return TimeSync.instance().offset;
    }

    sync_time_finish(data: TimeSyncData)
    {
        var c0 = data.client[0];
        var c1 = data.client[1];
        var c2 = data.client[2];
        var c3 = data.client[3];
        var s0 = data.server[0];
        var s1 = data.server[1];
        var s2 = data.server[2];
        var s3 = data.server[3];

        var rtt1 = (c1-c0 + s1-s0) / 4;      // how long it takes to get the package to server
        var offset1 = c1 - (s1 - rtt1);

        var rtt2 = (c3-c2 + s3-s2) / 4;      // how long it takes to get the package to server
        var offset2 = c2 - (s2 - rtt2);

        var offset = (offset1 + offset2) / 2;
        return Math.round(offset);
    }

    async sync_time(data: TimeSyncData) {
        if (! data) {
            data = { 'client': [Date.now()]} as TimeSyncData;
        }

        if (this.offset)
            return this.offset;

        const response = await fetch( '/api/v1/time-sync', {
            method: 'POST',
            headers: {
                'Content-Type': "application/json; charset=utf-8",
            },
            body: JSON.stringify(data)
        });

        if (!response.ok) {
            Notifications.showError({msg: `Failed to sync time! ${response.status}`})
        } else {
            const json = await response.json();

            if (json.client && json.server) {
                if (json.client.length < 4) {
                    json.client.push(Date.now());
                    this.sync_time(json);
                } else {
                    this.offset = this.sync_time_finish(json);
                    return this.offset;
                }
            }
        }
    }
}
