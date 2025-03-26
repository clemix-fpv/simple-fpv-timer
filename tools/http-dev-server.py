#!/usr/bin/python3.11

# SPDX-License-Identifier: GPL-3.0+

from simple_http_server import route, server
from simple_http_server import request_map
from simple_http_server import Response
from simple_http_server import MultipartFile
from simple_http_server import Parameter
from simple_http_server import Parameters
from simple_http_server import Header
from simple_http_server import JSONBody
from simple_http_server import HttpError
from simple_http_server import StaticFile
from simple_http_server import Headers
from simple_http_server import Cookies
from simple_http_server import Cookie
from simple_http_server import Redirect
from simple_http_server import ModelDict
from simple_http_server import PathValue
from simple_http_server import WebsocketHandler, WebsocketRequest,WebsocketSession, websocket_handler
import json
import simple_http_server.logger as logger
import traceback
import asyncio
from pathlib import Path
from random import randrange, shuffle
import time
import math
import os
from uuid import uuid4
import sys
import re
import random
import numpy

MAX_LAPS = 16
NUMBER_OF_PLAYERS = 3

class Player:
    lap_time = 0
    lap_count = 0

    def __init__(self, name, ipaddr = 0):
        self.laps = []
        self.name = name
        self.ipaddr = ipaddr
        self.lap_time = time.time()
        self.nextLap()

    def nextLap(self):
        duration = round((time.time() - self.lap_time) * 1000);
        if duration > randrange(30000) + 5000:
            self.lap_count += 1
            self.laps.append(
                    {'id': self.lap_count, 'duration': duration, 'abs_time': round(time.time() *1000), 'rssi': randrange(300) + 700 }
                    )
            while len(self.laps) > MAX_LAPS:
                self.laps.pop(0)
            self.lap_time = time.time()
            return True
        return False

    def addLap(self, duration, rssi):
        self.lap_count += 1
        print ("AddLap: {} {}".format(duration, rssi))
        self.laps.append(
                {'id': self.lap_count, 'duration': duration, 'abs_time': round(time.time() *1000), 'rssi': rssi}
            )
        while len(self.laps) > MAX_LAPS:
            self.laps.pop(0)


    def clearLaps(self):
        self.laps = [];
        self.lap_time = time.time();
        self.lap_count = 0;

    def json(self):
        return {
            'name': self.name,
            'ipaddr': self.ipaddr,
            'laps': self.laps
            }

    def dumpJson(self):
        return json.dumps(self.json(), indent = 1)

config = {
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
    "ctrl_port": 9090,

    #"game_mode": 0,
     "game_mode": 1,

    "led_num": 25,
    }

status = {
    "in_calib_mode":[0 for i in range(0,8)],
    "in_calib_lap_count": [0 for i in range(0,8)],


    # TODO obsolete clean this up!
    "rssi_smoothed":515,
    "rssi_raw":527,
    "rssi_peak":900,
    "rssi_enter":720,
    "rssi_leave":630,
    "drone_in_gate": False,

    'players': [ ]
    }


ctx = type('',(object,),{"players": [], 'send_rssi_updates': False, 'config': config, 'status': status, "boo": '3'})()




@websocket_handler(endpoint="/ws/{path_val}", singleton=True)
class WSHandler(WebsocketHandler):
    task = None
    race_ctx = None
    ctf_ctx = None

    def __init__(self) -> None:
        self.uuid: str = uuid4().hex


    def on_handshake(self, request: WebsocketRequest):
        return 0, {}

    def sendPlayers(self, session):
        json_data = {
            'type' : "players",
            'players' : [ x.json() for x in ctx.players ]
        }
        session.send(json.dumps(json_data))


    async def await_func(self, obj):
        if asyncio.iscoroutine(obj):
            return await obj
        return obj


    def random_player_update(self, session):

        if self.race_ctx is None:
            self.race_ctx = {
                "start": time.time(),
                "leave_time": time.time(),
                "enter_time": time.time(),
                "max_rssi": 0,
                "channels":  [
                {"channel" : 5917, "offset": 0, 'data': []},
                               {"channel" : 5695, "offset": 100, 'data': []}
                #    int freq_plan[8] = { 5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917 };
                ],
                "idx": 0,
            }

        need_update = False
        for x in ctx.players[1::]:
            if random.randint(1, 10) == 5:
                need_update = x.nextLap()

        if session.is_closed:
            print("Stop generate rssi values")
            return

        self.race_ctx['idx'] = (self.race_ctx['idx'] + 1) % len(self.race_ctx['channels'])
        chan = self.race_ctx['channels'][self.race_ctx['idx']]
        # print("Index {} ".format(chan))


        duration = time.time() - self.race_ctx['start']
        rssi = math.sin(math.pi * duration/10 + chan['offset']) * ctx.config['rssi[0].peak']
        if rssi < 200:
            rssi = 200
        rssi = randrange(int(rssi-100), int(rssi+100))
        ctx.status['rssi_raw'] = rssi

        f = ctx.config['rssi[0].filter'] / 100.0
        if f < 0.01:
            f = 0.01
        ctx.status['rssi_smoothed'] = int((f * rssi) + ((1.0-f)* ctx.status['rssi_smoothed']))

        if ctx.status['rssi_smoothed'] > ctx.status['rssi_enter'] and (time.time() - self.race_ctx['leave_time']) > 1 :
            if ctx.status['drone_in_gate'] == False:
                self.race_ctx['enter_time'] = time.time()
                self.race_ctx['max_rssi'] = 0
            ctx.status['drone_in_gate'] = True
            if self.race_ctx['max_rssi'] < ctx.status['rssi_smoothed']:
                self.race_ctx['max_rssi'] = ctx.status['rssi_smoothed']

        if ctx.status['rssi_smoothed'] < ctx.status['rssi_leave'] and ctx.status['drone_in_gate'] and (time.time() - self.race_ctx['enter_time']) > 1 :
            self.race_ctx['leave_time'] = time.time()
            ctx.status['drone_in_gate'] = False
            # print("leave:{} enter:{}".format(leave_time, self.race_ctx['enter_time']))
            ctx.players[0].addLap((int)((self.race_ctx['leave_time'] - self.race_ctx['enter_time']) * 1000), self.race_ctx['max_rssi'])
            need_update = True

        chan['data'].append({
                't': int(time.time() * 1000),
                'r': ctx.status['rssi_raw'],
                's': ctx.status['rssi_smoothed'],
                'i': ctx.status['drone_in_gate']
                })
        if (len(chan['data']) >= 3):
            json_data = {
                'type': "rssi",
                'freq': chan['channel'],
                'data': chan['data'],
            }
            if ctx.send_rssi_updates:
                session.send_text(json.dumps(json_data))
            chan['data'] = []

        if need_update:
            self.sendPlayers(session)


    def ctf_get_team_names(self):
        teams = []
        for o in ctx.config:
            m = re.match(r'rssi\[(\d+)\].name', o)
            if m:
                if ctx.config['rssi[{}].freq'.format(m.group(1))] != 0:
                    if len(ctx.config[o]) > 0:
                        teams.append(ctx.config[o])
        return teams

    def ctf_init(self):
        self.ctf_ctx = {}
        teams = self.ctf_get_team_names();

        self.ctf_ctx['time'] = time.time()
        self.ctf_ctx['json'] = {
            "team_names": teams,
            "nodes": [
                {
                    "name": "NodeA",
                    "ipv4": "0.0.0.0",
                    "current": 1,
                    "captured_ms": [ 0 for i in teams]
                },
                {
                    "name": "NodeB",
                    "ipv4": "192.168.2.22",
                    "current": 1,
                    "captured_ms": [ 0 for i in teams]
                },
                {
                    "name": "NodeC",
                    "ipv4": "192.168.2.23",
                    "current": 1,
                    "captured_ms": [ 0 for i in teams]
                },
            ]
        }

    def random_ctf_update(self, session):
        if self.ctf_ctx is None:
            self.ctf_init()
        elif (not (
                numpy.array(self.ctf_ctx['json']['team_names']) ==
                numpy.array(self.ctf_get_team_names())).all()):
            self.ctf_init()

        elapsed = time.time() - self.ctf_ctx['time']

        if (elapsed > 1):
            for n in self.ctf_ctx['json']['nodes']:

                idx = random.randint(1, len(n['captured_ms'])) - 1;
                n['current'] = idx;
                n['captured_ms'][idx] += (int)(elapsed * 1000)

            session.send(json.dumps({
                'type': 'ctf',
                'ctf': self.ctf_ctx['json']
            }))
            self.ctf_ctx['time'] = time.time();

    def random_spectrum_update(self, session):
        data = []
        data.append({
                't': int(time.time() * 1000),
                'r': randrange(500, 1200),
                's': randrange(500, 1200),
                'i': False
                })
        json_data = {
                'type': "rssi",
                'freq': ctx.config["rssi[0].freq"],
                'data': data,
            }
        session.send_text(json.dumps(json_data))

    async def periodic(self, session):
        try:
            while True:
                await asyncio.sleep(0.2)
                if (ctx.config['game_mode'] == 0):
                    self.random_player_update(session);

                elif ctx.config['game_mode'] == 1: # CTF
                    self.random_ctf_update(session);

                elif ctx.config['game_mode'] == 2: # SPECTRUM
                    self.random_spectrum_update(session);

        except Exception as e:
            print (e)
            traceback.print_exc()


    async def on_open(self, session: WebsocketSession):
        """
        "
        " Will be called when the connection opened.
        "
        """
        try:
            if self.task is not None:
                self.task.cancel()

            self.task = asyncio.create_task(self.periodic(session))
        except e:
            print(e);

    def on_close(self, session: WebsocketSession, reason: str):
        """
        "
        " Will be called when the connection closed.
        "
        """

    def on_ping_message(self, session: WebsocketSession = None, message: bytes = b''):
        """
        "
        " Will be called when receive a ping message. Will send all the message bytes back to client by default.
        "
        """
        session.send_pone(message)

    def on_pong_message(self, session: WebsocketSession = None, message: bytes = ""):
        """
        "
        " Will be called when receive a pong message.
        "
        """
        pass

    def on_text_message(self, session: WebsocketSession, message: str):
        """
        "
        " Will be called when receive a text message.
        "
        """
        session.send(message)


def save_config(obj):
    with open("config.json", 'w') as f:
        f.write(json.dumps(obj, indent=2))


def load_config():
    if not os.path.isfile("config.json"):
        return

    with open("config.json", 'r') as f:
        o = json.load(f)
        new_config = {}
        for key in o:
            if key in config:
                new_config[key] = o[key]
            else:
                print("Invalid key {} in configuration".format(key))
                return
        print("Configuration Successfull loaded!")
        ctx.config = new_config


@route("/api/v1/laps")
def index():
    return {"hello": "world"}

@route("/api/v1/settings", method="GET")
def handle_api_v1_settings():
    json_data = {
                'config' : ctx.config,
                'status' : ctx.status.copy()
            }
    json_data['status']['players'] = [ x.json() for x in ctx.players ]
    return json_data

@route("/api/v1/settings", method="POST")
def handle_api_v1_settings(json=JSONBody()):
    for key in json:
        print("KEY:{}".format(key))
        if key in config:
            print("{} = {} of {}".format(key, json[key], type(json[key])))
            ctx.config[key] = int(json[key]) if  json[key].isnumeric() else json[key];
        else:
            return {"status": "error", "msg": "Invalid key/value pair - unkown key {}".format(key)}


    save_config(ctx.config);
    #    ctx.config = json
    return {"status": "ok", 'config': ctx.config}


@request_map("/api/v1/player/lap", method=["POST"])
def handle_api_v1_player_connect(json=JSONBody()):
    for p in ctx.players :
        if p.name == json["player"] :
            l = json['lap']
            p.laps.append({
                'id': l['id'] ,
                'duration': l['duration'],
                'abs_time': round(time.time() *1000),
                'rssi': l['rssi']
            })

            return {'status':"ok", 'msg':"Lap added!"}
    return {'status':"failed", 'msg':"Player not found!"}

@request_map("/api/v1/player/connect", method=["POST"])
def handle_api_v1_player_connect(json=JSONBody()):
    for p in ctx.players :
        if p.name == json["player"]:
            return {'status':"failed", 'msg':"Player added!"}
    ctx.players.append(Player(json["player"]))
    return {'status':"ok", 'msg':"Player added!"}

@request_map("/api/v1/time-sync", method=["POST"])
def handle_api_v1_time(json=JSONBody()):
    if 'server' in json:
        json['server'].append(round(time.time() * 1000))
    else:
        json['server'] = [round(time.time() * 1000)]

    return json

@request_map("/api/v1/rssi/update", method=["POST"])
def handle_api_v1_time(json=JSONBody()):
    if 'enable' in json:
        ctx.send_rssi_updates = json['enable'] == 1

    return {'status':"ok"}

@request_map("/{file}", method="GET")
def default_static(file = PathValue()):

    content_type = {
                '.html': 'text/html',
                '.js': 'text/javascript',
                '.css': 'text/css',
                '.ogg': 'audio/ogg',
                '.ico': 'image/x-icon',
                '.svg': 'image/svg+xml',
                }

    root = os.path.dirname(os.path.abspath(__file__))
    path = Path("{}/../src/data_src".format(os.path.dirname(__file__)))
    res = path / file

    return StaticFile(res, "{}; charset=utf-8".format(content_type.get(res.suffix,"text/html")))

@request_map("/", method="GET")
def home():
    return default_static("index.html")


# TODO
if __name__ == "__main__":
    ip = "0.0.0.0"
    port = 9090
    names = ["Bob", "Teo", "Karla", "Joe", "Alice", "Fabs", "Foo", "Gerom", "Esta", "Karl Gustav the First"]
    shuffle(names)
#    logger.set_level("DEBUG")
    ctx.players = [Player(names.pop()) for x in range(NUMBER_OF_PLAYERS)]
    for x in ctx.players[1::]:
        x.ipaddr = "10.0.0.{}".format(randrange(100)+2)

    load_config()

    print("Browse to http://localhost:{}".format(port))
    server.start(port=port, prefer_coroutine=True)

