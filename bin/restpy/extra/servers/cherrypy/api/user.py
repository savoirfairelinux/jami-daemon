from bottle import request, response, post, get
import json, cherrypy

class User:
    def __init__(self, dring):
        self.dring = dring

    @cherrypy.expose
    def index(self):
        return 'todo'

    @cherrypy.expose
    def accounts(self):
        return json.dumps(self.dring.config.accounts())

    @cherrypy.expose
    def details(self, account_id):
        details = self.dring.config.account_details(str(account_id))
        return json.dumps(details)

    @cherrypy.expose
    def text(self, account_id, to_ring_id, message):
        self.dring.config.send_text_message(
                account_id, to_ring_id, {'text/plain': message})
