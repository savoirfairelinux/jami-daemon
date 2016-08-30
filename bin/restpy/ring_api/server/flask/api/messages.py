#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Authors:  Seva Ivanov <seva.ivanov@savoirfairelinux.com>
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

class Messages(Resource):
    def __init__(self, dring):
        self.dring = dring

    def get(self, message_id):

        status = None
        raw_status = self.dring.config.get_message_status(int(message_id))

        if (raw_status == 0):
            status = "UNKNOWN"

        elif (raw_status == 1):
            status = "SENDING"

        elif (raw_status == 2):
            status = "SENT"

        elif (raw_status == 3):
            status = "READ"

        elif (raw_status == 4):
            status = "FAILURE"

        if (not status):
            return jsonify({
                'status': 404,
                'message': 'unidentified message status'
            })

        return jsonify({
            'status': 200,
            'message_status': status
        })
