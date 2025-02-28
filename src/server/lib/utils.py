import time
import requests
import threading


def get_millis():
    return round(time.time() * 1000)


def __request(url, method="GET", data=None):
    resp = None
    if (method == "GET"):
        resp = requests.get(url)
    elif method == "POST" and data is not None:
        resp = requests.post(url, data)
    else:
        raise Exception("Invalid arguments: {}".format(data))

    print(f"{url}: {resp.status_code}")


def http_send(url, *args, **kwargs):
    data = kwargs.get('data', None)
    method = kwargs.get('method', 'GET')
    print(method + " | " + data)
    t = threading.Thread(target=__request, args=(url, method, data))
    t.start()
