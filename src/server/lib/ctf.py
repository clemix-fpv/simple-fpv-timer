import requests
import json
from node import Node
from utils import get_millis, http_send


class CTFNodeException(Exception):
    ipv4 = ""
    name = ""

    def __init__(self, ipv4, name):
        self.ipv4 = ipv4
        self.name = name


class CTFConfigDoesNotMatch(CTFNodeException):
    pass


class CTFNodeNotFound(CTFNodeException):
    pass


class CtfNode(Node):
    current = -1
    captured_ms = []

    def __init__(self, node):
        super().__init__(node.ipaddr, node.name)

    def reset(self):
        self.current = -1
        self.captured_ms = 0

    def toJsonObj(self):
        n = {}
        n['ipv4'] = self.ipaddr
        n['name'] = self.name
        n['current'] = self.current
        n['captured_ms'] = self.captured_ms
        return n


class Ctf:
    team_names = []
    nodes = []
    duration_ms = 0
    start_time = 0

    def findNode(self, ipv4) -> CtfNode:
        for n in self.nodes:
            if n.ipaddr == ipv4:
                return n
        return None

    def addNode(self, node):

        if self.findNode(node.ipaddr) is not None:
            return False

        self.nodes.append(CtfNode(node))
        return True

    def string_array_equal(self, a, b):
        sa = '|'.join(a)
        sb = '|'.join(b)
        return sa == sb

    def onUpdate(self, json):
        json_node = json['ctf']['nodes'][0]
        if json_node is None:
            raise Exception("Invalid JSON, missing node[0]")

        a1 = json['ctf']['team_names']
        a2 = self.team_names

        if not self.string_array_equal(a1, a2):
            raise CTFConfigDoesNotMatch(json_node['ipv4'], json_node['name'])

        node = self.findNode(json_node['ipv4'])
        if node is None:
            raise CTFNodeNotFound(json_node['ipv4'], json_node['name'])

        if self.isRunning():
            node.current = json_node['current']
            node.captured_ms = [i for i in json_node['captured_ms']]

    def onConfigChange(self, cfg):
        self.team_names = [v for v in cfg.getAllRssi("name").values()]
        self.nodes = []

    def start(self, duration_ms):
        self.start_time = get_millis()
        self.duration_ms = duration_ms

        for node in self.nodes:
            node.reset()
            url = "http://{}/api/v1/ctf/start".format(node.ipaddr)
            data = {'duration': duration_ms - (get_millis() - self.start_time)}
            http_send(url, method="POST", data=json.dumps(data))

        return {"status": "ok"}

    def stop(self):
        self.start_time = 0
        self.duration_ms = 0
        for node in self.nodes:
            url = "http://{}/api/v1/ctf/stop".format(node.ipaddr)
            http_send(url)
        return {"status": "ok"}

    def isRunning(self):
        if (self.start_time > 0):
            left = self.duration_ms - (get_millis() - self.start_time)
            return left > 0
        return False

    def toJsonObj(self) -> dict:
        obj = {}

        obj['team_names'] = self.team_names
        if (self.isRunning()):
            time_left = (get_millis() - self.start_time)
            obj['time_left_ms'] = self.duration_ms - time_left
        else:
            obj['time_left_ms'] = 0
        obj['nodes'] = []

        for node in self.nodes:
            obj['nodes'].append(node.toJsonObj())

        return obj
