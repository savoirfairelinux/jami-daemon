#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Authors:  Simon Zeni  <simon.zeni@savoirfairelinux.com>
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

class Tls(Resource):
    def __init__(self, dring):
        self.dring = dring

    def get(self):
        data = request.args
        if (not data):
            return jsonify({
                'status': 404,
                'message': 'data not found'
            })

        elif ('type' not in data):
            return jsonify({
                'status': 404,
                'message': 'type not found in data'
            })

        tls_type = data.get('type')

        if (tls_type == 'settings'):
            return jsonify({
                'status': 200,
                'settings': self.dring.config.get_tls_default_settings()
            })

        elif (tls_type == 'method'):
            return jsonify({
                'status': 200,
                'methods': self.dring.config.get_supported_tls_method()
            })

        return jsonify({
            'status': 400,
            'message': 'wrong tls type'
        })
