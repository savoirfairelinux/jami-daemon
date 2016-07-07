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

class VideoDevices(Resource):
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

        device_type = data.get('type')

        if (device_type == 'all'):
            return jsonify({
                'status': 200,
                'devices': self.dring.video.devices()
            })

        elif (device_type == 'default'):
            return jsonify({
                'status': 200,
                'default': self.dring.video.get_default_device()
            })

        return jsonify({
            'status': 400,
            'message': 'wrong device type'
        })

    def put(self):
        data = request.get_json(force=True)

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

        device_type = data['type']

        if (device_type == 'default'):
            data = request.get_json(force=True)

            if ('device' not in data):
                return jsonify({
                    'status': 400,
                    'message': 'device not found in request data'
                })

            self.dring.video.set_default_device(data['device'])

            return jsonify({
                'status': 200,
                'default': self.dring.video.get_default_device()
            })

        return jsonify({
            'status': 400,
            'message': 'wrong device type'
        })


class VideoSettings(Resource):
    def __init__(self, dring):
        self.dring = dring

    def get(self, device_name):
        return jsonify({
            'status': 200,
            'settings': self.dring.video.get_settings(device_name)
        })

    def put(self, device_name):
        data = request.get_json(force=True)

        if ('settings' not in data):
            return jsonify({
                'status': 404,
                'message': 'settings not found'
            })

        self.dring.video.apply_settings(device_name, data['settings'])

        return jsonify({
            'status': 200,
            'settings': self.dring.video.get_settings(device_name)
        })


class VideoCamera(Resource):
    def __init__(self, dring):
        self.dring = dring

    def get(self):
        return jsonify({
            'status': 200,
            'cameraStatus': self.dring.video.has_camera_started()
        })

    def put(self):
        data = request.get_json(force=True)

        if (data['action'] == 'start'):
            self.dring.video.start_camera()

            return jsonify({
                'status': 200,
                'cameraStatus': self.dring.video.has_camera_started()
            })

        elif (data['action'] == 'stop'):
            self.dring.video.stop_camera()

            return jsonify({
                'status': 200,
                'cameraStatus': self.dring.video.has_camera_started()
            })

        return jsonify({
            'status': 404,
            'message': 'wrong camera action'
        })
