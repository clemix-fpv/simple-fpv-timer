from pathlib import Path
import os
import json
import re

class Config:
    MAX_RSSI = 8
    data = {
        "rssi[0].name" : "",
        "rssi[0].freq": 5917,
        "rssi[0].peak": 900,
        "rssi[0].filter": 60,
        "rssi[0].offset_enter": 80,
        "rssi[0].offset_leave": 70,
        "rssi[0].calib_max_lap_count": 3,
        "rssi[0].calib_min_rssi_peak": 600,
        "rssi[0].led_color" : 14876421,

        "rssi[1].name" : "FOO",
        "rssi[1].freq": 5923,
        "rssi[1].peak": 999,
        "rssi[1].filter": 66,
        "rssi[1].offset_enter": 88,
        "rssi[1].offset_leave": 77,
        "rssi[1].calib_max_lap_count": 3,
        "rssi[1].calib_min_rssi_peak": 666,
        "rssi[1].led_color" : 255,

        "rssi[2].name" : "",
        "rssi[2].freq": 0,
        "rssi[2].peak": 900,
        "rssi[2].filter": 60,
        "rssi[2].offset_enter": 80,
        "rssi[2].offset_leave": 70,
        "rssi[2].calib_max_lap_count": 3,
        "rssi[2].calib_min_rssi_peak": 600,
        "rssi[2].led_color" : 255,

        "rssi[3].name" : "",
        "rssi[3].freq": 0,
        "rssi[3].peak": 900,
        "rssi[3].filter": 60,
        "rssi[3].offset_enter": 80,
        "rssi[3].offset_leave": 70,
        "rssi[3].calib_max_lap_count": 3,
        "rssi[3].calib_min_rssi_peak": 600,
        "rssi[3].led_color" : 255,

        "rssi[4].name" : "",
        "rssi[4].freq": 5917,
        "rssi[4].peak": 900,
        "rssi[4].filter": 60,
        "rssi[4].offset_enter": 80,
        "rssi[4].offset_leave": 70,
        "rssi[4].calib_max_lap_count": 3,
        "rssi[4].calib_min_rssi_peak": 600,
        "rssi[4].led_color" : 255,

        "rssi[5].name" : "",
        "rssi[5].freq": 0,
        "rssi[5].peak": 900,
        "rssi[5].filter": 60,
        "rssi[5].offset_enter": 80,
        "rssi[5].offset_leave": 70,
        "rssi[5].calib_max_lap_count": 3,
        "rssi[5].calib_min_rssi_peak": 600,
        "rssi[5].led_color" : 255,

        "rssi[6].name" : "",
        "rssi[6].freq": 0,
        "rssi[6].peak": 900,
        "rssi[6].filter": 60,
        "rssi[6].offset_enter": 80,
        "rssi[6].offset_leave": 70,
        "rssi[6].calib_max_lap_count": 3,
        "rssi[6].calib_min_rssi_peak": 600,
        "rssi[6].led_color" : 255,

        "rssi[7].name" : "",
        "rssi[7].freq": 0,
        "rssi[7].peak": 900,
        "rssi[7].filter": 60,
        "rssi[7].offset_enter": 80,
        "rssi[7].offset_leave": 70,
        "rssi[7].calib_max_lap_count": 3,
        "rssi[7].calib_min_rssi_peak": 600,
        "rssi[7].led_color" : 255,

        "elrs_uid": "0,0,0,0,0,0",
        "osd_x": 0,
        "osd_y": 0,
        "osd_format": "%2L: %5.2ts(%6.2ds)",
        "wifi_mode": 0,
        "ssid": "clemixfpv",
        "passphrase": "",
        "node_name": "",
        "node_mode": 0,
        "ctrl_ipv4": "0.0.0.0",
        "ctrl_port": "80",

        #"game_mode": 0,
         "game_mode": 1,

        "led_num": 25,
    }


    def game_mode(self):
        return self.data["game_mode"];


    def getAll(self, regex):
        ret = dict()
        for key in self.data:
            if re.fullmatch(regex, key):
                ret[key] = self.data[key]
        return ret


    def getAllRssi(self, key):
        ret = dict()
        for i in range(0, self.MAX_RSSI):
            if self.data['rssi[{}].freq'.format(i)] == 0:
                continue
            fullkey = 'rssi[{}].{}'.format(i, key)
            ret[fullkey] = self.data[fullkey]
        return ret


    def save(self):
        with open("config.json", 'w') as f:
            f.write(json.dumps(self.data, indent=2))


    def load(self):
        if not os.path.isfile("config.json"):
            return False

        with open("config.json", 'r') as f:
            try:
                o = json.load(f)
                new_config = {}
                for key in o:
                    if key in self.data:
                        new_config[key] = o[key]
                    else:
                        print("Invalid key {} in configuration".format(key))
                        return
                print("Configuration Successfull loaded!")
                self.data = new_config
                return True
            except Exception as e:
                print("ERROR on loading config file!")
                print(e)

        return False

