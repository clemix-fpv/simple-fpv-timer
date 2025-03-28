from config import Config
from node import Node
from ctf import CTFConfigDoesNotMatch, CTFNodeNotFound, Ctf
from race import Race, RacePlayerNotFound
from utils import http_send
import json
from enum import IntEnum
import functools


class GameMode(IntEnum):
    RACE = 0
    CTF = 1
    SPECTRUM = 2


def race_only(func):
    @functools.wraps(func)
    def wrapper(self, *args, **kwargs):
        if self.getGameMode() == GameMode.RACE:
            return func(self, *args, **kwargs)
        else:
            return {"status": "error", "msg": "Expect RACE mode"}
    return wrapper


def ctf_only(func):
    @functools.wraps(func)
    def wrapper(self, *args, **kwargs):
        if self.getGameMode() == GameMode.CTF:
            return func(self, *args, **kwargs)
        else:
            return {"status": "error", "msg": "Expect RACE mode"}
    return wrapper


class Ctx:
    config = Config()
    nodes = []
    ctf = Ctf()
    race = Race()

    def getGameMode(self) -> GameMode:
        if (self.config.game_mode() == 1):
            return GameMode.CTF
        elif (self.config.game_mode() == 2):
            return GameMode.SPECTRUM
        return GameMode.RACE

    def findNode(self, ip4: str) -> Node:
        for node in self.nodes:
            if (node.ipaddr == ip4):
                return node
        return None

    def addNode(self, ip4, name, player):

        node = self.findNode(ip4)
        if node is None:
            node = Node(ip4, name)
            self.nodes.append(node)
        else:
            node.name = name
            node.updateLastseen()

        if self.getGameMode() == GameMode.CTF:
            if self.ctf.addNode(node):
                self.sendRssiConfigCTF(ip4)

        elif self.getGameMode() == GameMode.RACE:
            if self.race.addPlayer(ip4, player):
                self.updateNodes()

        return True

    def sendRssiConfigCTF(self, ipv4):

        cfg = dict()
        for i in self.config.data:
            if (i.startswith('rssi[')):
                if (i.endswith("name") or
                        i.endswith("freq") or
                        i.endswith("peak") or
                        i.endswith("filter") or
                        i.endswith("offset_enter") or
                        i.endswith("offset_leave") or
                        i.endswith("led_color")):
                    cfg[i] = self.config.data[i]

        cfg["game_mode"] = self.config.data["game_mode"]
        cfg["led_num"] = self.config.data["led_num"]

        url = "http://{}/api/v1/settings".format(ipv4)

        http_send(url, method="POST", data=json.dumps(cfg))

    def sendRssiConfigRace(self, ipv4, idx):
        cfg = dict()
        for i in self.config.data:
            if (i.startswith('rssi[{}]'.format(idx))):
                if (i.endswith("name") or
                        i.endswith("freq") or
                        i.endswith("peak") or
                        i.endswith("filter") or
                        i.endswith("offset_enter") or
                        i.endswith("offset_leave") or
                        i.endswith("led_color")):
                    new_key = i.replace('rssi[{}]'.format(idx), 'rssi[0]')
                    cfg[new_key] = self.config.data[i]

        cfg["game_mode"] = self.config.data["game_mode"]
        cfg["led_num"] = self.config.data["led_num"]

        url = "http://{}/api/v1/settings".format(ipv4)

        http_send(url, method="POST", data=json.dumps(cfg))

    def updateNodes(self):
        idx = 0
        for n in self.nodes:
            if self.getGameMode() == GameMode.CTF:
                self.sendRssiConfigCTF(n.ipaddr)
            else:
                self.sendRssiConfigRace(n.ipaddr, idx)
            idx = idx + 1

    def save_config(self, json):
        for key in json:
            print("KEY:{}".format(key))
            if key in self.config.data:
                print("{} = {} of {}".format(key, json[key], type(json[key])))
                if json[key].isnumeric():
                    self.config.data[key] = int(json[key])
                else:
                    json[key]
            else:
                return False

        self.config.save()
        self.updateNodes()

        if self.getGameMode() == GameMode.CTF:
            self.ctf.onConfigChange(self.config)

        if self.getGameMode() == GameMode.RACE:
            self.race.onConfigChange(self.config)

        return True

    def load_config(self):
        if (self.config.load()):
            self.ctf.onConfigChange(self.config)

    @ctf_only
    def onCTFUpdate(self, json):

        ###
        # json = {'type': 'ctf',
        #     'ctf': {'team_names': [],
        #             'nodes': [
        #                       {
        #                          'ipv4': '192.168.2.173',
        #                          'name': 'ON_THE_TABLE',
        #                          'current': -1,
        #                          'captured_ms': []
        #        }]}}

        try:
            json_node = json['ctf']['nodes'][0]
            if json_node is not None:
                node = self.findNode(json_node['ipv4'])
                if node is not None:
                    node.updateLastseen()

            self.ctf.onUpdate(json)
        except CTFConfigDoesNotMatch as e:
            print("CTFConfigDoesNotMatch:" + e.ipv4)
            self.sendRssiConfig(e.ipv4)
            return {'status': 'error', 'msg': 'Config invalid'}
        except CTFNodeNotFound as e:
            print("CTFNodeNotFound:" + e.ipv4)
            self.addNode(e.ipv4, e.name)
            return {'status': 'error', 'msg': 'Node not found, try again'}
        except Exception as e:
            print(e)

        return {'status': 'ok', 'msg': ''}

    @ctf_only
    def onCTFStart(self, json):
        return self.ctf.start(json['duration_ms'])

    @ctf_only
    def onCTFStop(self):
        return self.ctf.stop()

    @race_only
    def onRaceLap(self, json):
        try:
            self.race.addLap(json)
        except RacePlayerNotFound as e:
            self.addNode(e.ipv4, e.name)
        return {'status': 'ok', 'msg': ''}

    @race_only
    def onRaceClearLaps(self, offset):
        self.race.startRace(offset)
