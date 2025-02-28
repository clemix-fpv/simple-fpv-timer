from utils import get_millis, http_send
import json
from typing import TypeAlias


class RacePlayerNotFound(Exception):
    ipv4 = ""
    name = ""

    def __init__(self, ipv4, name):
        self.ipv4 = ipv4
        self.name = name


class Lap:
    id = 0
    duration = 0
    rssi = 0
    abs_time = 0

    def __init(self, id, duration, rssi, abs_time=None):
        self.id = id
        self.duration = duration
        self.rssi = rssi
        if abs_time is None:
            abs_time = get_millis()
        self.abs_time = abs_time

    def toJsonObj(self):
        return self.__dict__.copy()


LapsList: TypeAlias = list[Lap]


class Player:
    name = ""
    ipv4 = ""
    laps: LapsList = list()

    def __init__(self, ipv4, name):
        self.name = name
        self.ipv4 = ipv4

    def addLap(self, lap):
        self.laps.append(lap)

    def reset(self):
        self.laps.clear()

    def toJsonObj(self):
        return {
            'name': self.name,
            'ipaddr': self.ipv4,
            'laps': [lap.toJsonObj() for lap in self.laps]
        }


PlayersList: TypeAlias = list[Player]


class Race:
    players: PlayersList = list()

    def findPlayer(self, ip) -> Player:
        for p in self.players:
            if p.ipv4 == ip:
                return p
        return None

    def onRaceLap(self, json):
        player = self.findPlayer(json['ipv4'])
        if player is None:
            raise RacePlayerNotFound(json['ipv4'], json['player'])

        player.addLap(Lap(json['id'], json['duration'], json['rssi']))
        return {'status': "ok"}

    def addPlayer(self, ipv4, name):
        p = self.findPlayer(ipv4)
        if p is not None:
            return False
        self.players.append(Player(ipv4, name))
        return True

    def startRace(self, delay_ms):

        for p in self.players:
            url = "http://{}/api/v1/clear_laps".format(p.ipv4)
            data = {'offset': delay_ms}
            http_send(url, method="POST", data=json.dumps(data))

        for p in self.players:
            p.reset()

    def onConfigChange(self, config):
        pass

    def toJsonObj(self):
        j = [p.toJsonObj() for p in self.players]
        return j
