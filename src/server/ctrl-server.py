#!/usr/bin/python3.11

# SPDX-License-Identifier: GPL-3.0+
from simple_http_server import route, server
from simple_http_server import request_map
from simple_http_server import JSONBody
from simple_http_server import StaticFile
from simple_http_server import PathValue
from simple_http_server import WebsocketHandler, WebsocketRequest
from simple_http_server import WebsocketSession, websocket_handler
import json
import traceback
import asyncio
from pathlib import Path
import time
import os
from uuid import uuid4
import sys
from os.path import dirname

sys.path.append("{}/lib/".format(dirname(__file__)))
from lib.ctx import Ctx, GameMode


ctx = Ctx()


@websocket_handler(endpoint="/ws/{path_val}", singleton=True)
class WSHandler(WebsocketHandler):
    task = None

    def __init__(self) -> None:
        self.uuid: str = uuid4().hex

    def on_handshake(self, request: WebsocketRequest):
        return 0, {}

    async def await_func(self, obj):
        if asyncio.iscoroutine(obj):
            return await obj
        return obj

    async def periodic(self, session):
        try:
            while True:
                await asyncio.sleep(1)

                if ctx.getGameMode() == GameMode.CTF:
                    print("Send CTF update")
                    session.send(
                        json.dumps({"type": "ctf", "ctf": ctx.ctf.toJsonObj()})
                    )

                if ctx.getGameMode() == GameMode.RACE:
                    session.send(
                        json.dumps({"type": "players",
                                    "players": ctx.race.toJsonObj()})
                    )

        except Exception as e:
            print(e)
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
        except Exception as e:
            print(e)

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


@route("/api/v1/settings", method="GET")
def handle_api_v1_settings_GET():
    json_data = {
                'config': ctx.config.data,
            }
    return json_data


@route("/api/v1/settings", method="POST")
def handle_api_v1_settings_POST(json=JSONBody()):
    if ctx.save_config(json):
        return {"status": "ok", 'config': ctx.config.data}
    else:
        return {"status": "error", "msg": "Invalid key/value pair"}


@request_map("/api/v1/player/lap", method=["POST"])
def handle_api_v1_player_lap(json=JSONBody()):
    return ctx.onRaceLap(json)
    print("api/v1/player/lap")


@request_map("/api/v1/ctf/update", method=["POST"])
def handle_api_v1_ctf_update(json=JSONBody()):
    return ctx.onCTFUpdate(json)


@request_map("/api/v1/ctf/start", method=["POST"])
def handle_api_v1_ctf_start(json=JSONBody()):
    return ctx.onCTFStart(json)


@request_map("/api/v1/ctf/stop", method=["GET"])
def handle_api_v1_ctf_stop():
    return ctx.onCTFStop()


@request_map("/api/v1/clear_laps", method=["POST"])
def handle_api_v1_clear_laps(json=JSONBody()):
    return ctx.onRaceClearLaps(json['offset'] if 'offset' in json else 30000)


@request_map("/api/v1/player/connect", method=["POST"])
def handle_api_v1_player_connect(json=JSONBody()):

    if (ctx.addNode(json["ip4"], json["name"], json["player"] or "")):
        return {'status': "ok", 'msg': "Node added!"}
    else:
        return {'status': "error", 'msg': "Failed to add Node"}


@request_map("/api/v1/nodes", method=["GET"])
def handle_api_v1_nodes():
    return {'nodes': [i.__dict__ for i in ctx.nodes]}


@request_map("/api/v1/time-sync", method=["POST"])
def handle_api_v1_time(json=JSONBody()):
    if 'server' in json:
        json['server'].append(round(time.time() * 1000))
    else:
        json['server'] = [round(time.time() * 1000)]

    return json


@request_map("/api/v1/rssi/update", method=["POST"])
def handle_api_v1_rssi_update(json=JSONBody()):
    return {'status': "error", 'msg': "RSSI update not implemented"}

@request_map("/api/v1/rssi/update", method=["GET"])
def handle_api_v1_rssi_update():
    return {'enable': False}

@request_map("/{file}", method="GET")
def default_static(file=PathValue()):

    content_type = {
                '.html': 'text/html',
                '.js': 'text/javascript',
                '.css': 'text/css',
                '.ogg': 'audio/ogg',
                '.ico': 'image/x-icon',
                '.svg': 'image/svg+xml',
                }

    path = Path("{}/www".format(os.path.dirname(__file__)))
    res = path / file
    content_type = content_type.get(res.suffix, "text/html")
    return StaticFile(res, "{}; charset=utf-8".format(content_type))


@request_map("/", method="GET")
def home():
    return default_static("index.html")


# TODO
if __name__ == "__main__":
    port = 9090

    ctx.load_config()

    print("Browse to http://localhost:{}".format(port))
    server.start(port=port, prefer_coroutine=True)
