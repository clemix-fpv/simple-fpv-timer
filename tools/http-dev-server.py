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

    def clearLaps(self):
        self.laps = [];
        self.lap_time = time.time();
        self.lap_count = 0;

    def json(self):
        self.nextLap()
        return {
            'name': self.name,
            'ipaddr': self.ipaddr,
            'laps': self.laps
            }

    def dumpJson(self):
        return json.dumps(self.json(), indent = 1)

config = {
    "freq": 5917,
    "rssi_peak": 900,
    "rssi_filter": 60,
    "rssi_offset_enter": 80,
    "rssi_offset_leave": 70,
    "calib_max_lap_count": 3,
    "calib_min_rssi_peak": 600,
    "elrs_uid": "0,0,0,0,0,0",
    "osd_x": 0,
    "osd_y": 0,
    "player_name": "client2",
    "wifi_mode": 0,
    "ssid": "clemixfpv",
    "passphrase": ""
    }

status = {
    "rssi_smoothed":515,
    "rssi_raw":527,
    "rssi_peak":900,
    "rssi_enter":720,
    "rssi_leave":630,
    "drone_in_gate":False,
    "in_calib_mode":False,
    "in_calib_lap_count":0,
    'players': [ ]
    }


ctx = type('',(object,),{"players": [], 'config': config, 'status': status, "boo": '3'})()




@websocket_handler(endpoint="/ws/{path_val}", singleton=True)
class WSHandler(WebsocketHandler):
    task = None

    def __init__(self) -> None:
        self.uuid: str = uuid4().hex


    def on_handshake(self, request: WebsocketRequest):
        """
        "
        " You can get path/headers/path_values/cookies/query_string/query_parameters from request.
        "
        " You should return a tuple means (http_status_code, headers)
        "
        " If status code in (0, None, 101), the websocket will be connected, or will return the status you return.
        "
        " All headers will be send to client
        "
        """
#Upgrade: websocket
#Connection: Upgrade
        return 0, {}

    async def await_func(self, obj):
        if asyncio.iscoroutine(obj):
            return await obj
        return obj

    async def periodic(self, session):
        try:
            start = time.time()
            leave_time = time.time()
            enter_time = time.time()

            json_data = []

            while True:
                await asyncio.sleep(0.2)
                if session.is_closed:
                    print("Stop generate rssi values")
                    return

                duration = time.time() - start
                rssi = math.sin(math.pi * duration/10) * ctx.config['rssi_peak']
                if rssi < 200:
                    rssi = 200
                rssi = randrange(int(rssi-100), int(rssi+100))
                ctx.status['rssi_raw'] = rssi

                f = ctx.config['rssi_filter'] / 100.0
                if f < 0.01:
                    f = 0.01
                ctx.status['rssi_smoothed'] = int((f * rssi) + ((1.0-f)* ctx.status['rssi_smoothed']))

                if ctx.status['rssi_smoothed'] > ctx.status['rssi_enter'] and (time.time() - leave_time) > 1 :
                    ctx.status['drone_in_gate'] = True
                    enter_time = time.time()
                if ctx.status['rssi_smoothed'] < ctx.status['rssi_leave'] and ctx.status['drone_in_gate'] and (time.time() - enter_time) > 1 :
                    leave_time = time.time()
                    ctx.status['drone_in_gate'] = False

                json_data.append({
                        't': int(time.time() * 1000),
                        'r': ctx.status['rssi_raw'],
                        's': ctx.status['rssi_smoothed'],
                        'i': ctx.status['drone_in_gate']
                        })
                if (len(json_data) >= 3):
                    session.send_text(json.dumps(json_data))
                    json_data = []
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


@route("/api/v1/laps")
def index():
    return {"hello": "world"}

@route("/api/v1/settings", method=["GET", "POST"])
def handle_api_v1_settings():
    json_data = {
                'config' : ctx.config,
                'status' : ctx.status.copy()
            }
    json_data['status']['players'] = [ x.json() for x in ctx.players ]
    return json_data

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


if __name__ == "__main__":
    ip = "0.0.0.0"
    port = 9090
    names = ["Bob", "Teo", "Karla", "Joe", "Alice", "Fabs", "Foo", "Gerom", "Esta", "Karl Gustav the First"]
    shuffle(names)
#    logger.set_level("DEBUG")
    ctx.players = [Player(names.pop()) for x in range(NUMBER_OF_PLAYERS)]
    for x in ctx.players[1::]:
        x.ipaddr = "10.0.0.{}".format(randrange(100)+2)

    print("Browse to http://localhost:{}".format(port))
    server.start(port=port, prefer_coroutine=True)

