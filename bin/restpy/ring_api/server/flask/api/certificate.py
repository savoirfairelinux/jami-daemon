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

class Certificates(Resource):
    def __init__(self, dring):
        self.dring = dring

    def get(self, cert_id=None):
        if (cert_id):
            return jsonify({
                'status': 200,
                'details': self.dring.config.get_certificate_details(cert_id)
            })

        return jsonify({
            'status': 200,
            'pinned': self.dring.config.get_pinned_certificates()
        })

    def post(self, cert_id):
        data = request.get_json(force=True)

        if (not data):
            return jsonify({
                'status': 404,
                'message': 'data not found'
            })

        if ('action' not in data):
            return jsonify({
                'status': 400,
                'message': 'action not found in request data'
            })

        result = None

        if (data.get('action') == 'pin'):
            # temporary
            local = True if data.get('local') in ["True", "true"] else False

            result = self.dring.config.pin_certificate(cert_id, local)

        elif (data.get('action') == 'unpin'):
            result = self.dring.config.unpin_certificate(cert_id)

        else:
            return jsonify({
                'status': 400,
                'message': 'wrong action type'
            })

        return jsonify({
            'status': 200,
            'action': result
        })
