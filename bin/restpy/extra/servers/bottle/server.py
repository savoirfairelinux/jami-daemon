from bottle import run

from ring_api.server.bottle.api import root, ring, user

class BottleServer:

    def __init__(self, host, port, dring):
        self.host = host
        self.port = port
        self.dring = dring

        root.Root(dring)
        ring.Ring(dring)
        user.User(dring)

    def start(self):
        run(host=self.host, port=self.port)

    def stop(self):
        pass
