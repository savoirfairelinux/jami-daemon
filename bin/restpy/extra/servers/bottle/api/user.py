from bottle import request, response, post, get
import cgi, json

api = {
    'routes' : '/user/routes/',
    'accounts' : '/user/accounts/',
    'account_details': '/user/details/<account_id>/',
    'send_text_message': '/user/send/text/<account_id>/<to_ring_id>/<message>/',
    'devices' : '/user/video/devices'
}

class User:
    def __init__(self, _dring):
        global dring
        dring = _dring

    def api():
        return api

@get(api['routes'])
def routes():
    html = {**api}
    return html

# Configuration Manager
@get(api['accounts'])
def accounts():
    return json.dumps(dring.config.accounts())

@get(api['account_details'])
def account_details(account_id):
    details = dring.config.account_details(str(account_id))
    return json.dumps(details)

@post(api['send_text_message'])
def send_text_message(account_id, to_ring_id, message):
    dring.config.send_text_message(
            account_id, to_ring_id, {'text/plain': message})

# Video Manager
@get(api['devices'])
def devices():
    return json.dumps(dring.video.devices())
