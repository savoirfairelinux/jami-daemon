import cherrypy, json

from bottle import request, get

from ring_api.server.api import ring, user

class Root(object):
    def __init__(self, dring):
        self.dring = dring
        self.user = user.User(dring)

    @cherrypy.expose
    def index(self):
        return 'todo'

    @cherrypy.expose
    def routes(self):
        return 'todo'
