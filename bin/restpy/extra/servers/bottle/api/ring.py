from bottle import request, response, post, get
import cgi, json

api = {
    'routes' : '/ring/routes/',
    'version' : '/ring/version/'
}

class Ring:
    def __init__(self, _dring):
        global dring
        dring = _dring

    def api():
        return api

@get(api['routes'])
def routes():
    return json.dumps(api)

@get(api['version'])
def version():
    return json.dumps(dring.version())
