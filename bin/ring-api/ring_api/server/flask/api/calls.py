#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Authors:  Seva Ivanov <seva.ivanov@savoirfairelinux.com>
#           Simon Zeni  <simon.zeni@savoirfairelinux.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
#

from flask import jsonify, request
from flask_restful import Resource

class Calls(Resource):
    def __init__(self, dring):
        self.dring = dring

    def put(self, call_id):
        data = request.get_json(force=True)

        if (not 'action' in data):
            return jsonify({
                'status': 400,
                'message': 'action not found in request data'
            })

        if (data['action'] in 'accept'):
            return jsonify({
                'status': 200,
                'accept': self.dring.call.accept(call_id)
            })
        
        elif (data['action'] in 'refuse'):
            return jsonify({
                'status': 200,
                'refuse': self.dring.call.refuse(call_id)
            })
        
        elif (data['action'] in 'hangup'):
            return jsonify({
                'status': 200,
                'hang_up': self.dring.call.hang_up(call_id)
            })
        
        elif (data['action'] in 'hold'):
            return jsonify({
                'status': 200,
                'hold': self.dring.call.hold(call_id)
            })
        
        elif (data['action'] in 'unhold'):
            return jsonify({
                'status': 200,
                'unhold': self.dring.call.unhold(call_id)
            })
        
        else:
            return jsonify({
                'status': 400,
                'message': 'action not valid'
            })
