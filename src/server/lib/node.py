import json
from utils import get_millis


class Node:
    ipaddr = 0
    name = ""
    last_seen = 0

    def __init__(self, ipaddr, name):
        self.ipaddr = ipaddr
        self.name = name
        self.last_seen = get_millis()

    def updateLastseen(self):
        self.last_seen = get_millis()

    def __str__(self):
        return "Node: name:" + self.name + " ipaddr:" + self.ipaddr

    def toJSON(self):
        return json.dumps(
            self,
            default=lambda o: o.__dict__,
            sort_keys=True,
            indent=4)
