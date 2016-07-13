import json

from bottle import request, get

from ring_api.server.bottle.api import ring, user

api = {
    'routes': '/routes/',
    'all_routes': '/all_routes/',
}

class Root:
    def __init__(self, _dring):
        global dring
        dring = _dring

    def api():
        return api

@get(api['routes'])
def root():
    html = {**api}
    return html

@get(api['all_routes'])
def routes():
    ring_api = ring.Ring.api()
    user_api = user.User.api()
    html = {**api, **ring_api, **user_api}
    return html
