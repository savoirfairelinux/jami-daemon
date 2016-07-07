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

class Account(Resource):
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
        
        account_type = data.get('type')

        if (account_type == 'SIP'):
            return jsonify({
                'success': 200,
                'details': self.dring.config.get_account_template('SIP')
            })

        elif (account_type == 'IAX'):
            return jsonify({
                'success': 200,
                'details': self.dring.config.get_account_template('IAX')
            })

        elif (account_type == 'IP2IP'):
            return jsonify({
                'success': 200,
                'details': self.dring.config.get_account_template('IP2IP')
            })

        elif (account_type == 'RING'):
            return jsonify({
                'success': 200,
                'details': self.dring.config.get_account_template('RING')
            })

        return jsonify({
            'status': 400,
            'message': 'wrong account type'
        })

    def post(self):
        data = request.get_json(force=True)

        if (not 'details' in data):
            return jsonify({
                'status': 400,
                'message': 'details not found in request data'
            })

        return jsonify({
            'success': 200,
            'account_id': self.dring.config.add_account(data['details'])
        })
        
class Accounts(Resource):
    def __init__(self, dring):
        self.dring = dring

    def get(self):
        return jsonify({
            'success': 200,
            'accounts': self.dring.config.accounts()
        })

class AccountsID(Resource):
    def __init__(self, dring):
        self.dring = dring

    def delete(self, account_id):
        self.dring.config.remove_account(account_id)

        return jsonify({
            'status': 200,
            'accounts': self.dring.config.accounts()
        })

class AccountsDetails(Resource):
    def __init__(self, dring):
        self.dring = dring

    def get(self, account_id):
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
        
        account_type = data.get('type')

        if (account_type == 'default'):
            return jsonify({
                'status': 200,
                'details': self.dring.config.account_details(account_id)
            })

        elif (account_type == 'volatile'):
            pass

        return jsonify({
            'status': 400,
            'message': 'wrong account type'
        })

class AccountsCodecs(Resource):
    def __init__(self, dring):
        self.dring = dring

    def get(self, account_id, codec_id=None):
        if (codec_id is not None):
            return jsonify({
                'status': 200,
                'details': self.dring.config.get_codec_details(account_id, int(codec_id))
            })
        
        return jsonify({
            'status': 200,
            'details': self.dring.config.get_active_codec_list(account_id)
        })
    
    def put(self, account_id, codec_id=None):
        data = request.get_json(force=True)
        
        if (codec_id is not None):
            self.dring.config.set_codec_details(account_id, int(codec_id), data['details'])

            return jsonify({
                'status': 200,
                'details': self.dring.config.get_codec_details(account_id, int(codec_id))
            })
        
        self.dring.config.set_active_codec_list(account_id, codec_id, data['list'])

        return jsonify({
            'status': 200,
            'details': self.dring.config.get_active_codec_list(account_id)
        })
        

class AccountsCall(Resource):
    def __init__(self, dring):
        self.dring = dring

    def post(self, account_id):
        # Check if the account is valid and exists
        if (len(account_id) != 16):
            return jsonify({
                'status': 400,
                'message': 'account_id not valid'
            })

        accounts = self.dring.config.accounts()
        if (not any(account_id in account for account in accounts)):
            return jsonify({
                'status': 400,
                'message': 'account not found'
            })
        
        data = request.get_json(force=True)

        if (not 'ring_id' in data):
            return jsonify({
                'status': 400,
                'message': 'ring_id not found in request data'
            })
        
        return jsonify({
            'status': 200,
            'call_id': self.dring.call.place_call(account_id, data['ring_id'])
        })

class AccountsCertificates(Resource):
    def __init__(self, dring):
        self.dring = dring
   
    def get(self, account_id):
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
        
        action = data.get('type')

        if (action == 'validate'):
            return jsonify({
                'status': 200,
                'certificates': self.dring.config.validate_certificate(
                                    account_id, 
                                    cert_id
                                )
            })

        return jsonify({
            'status': 400,
            'message': 'wrong account type'
        })

    def put(self, account_id):
        data = request.args
        if (not data):
            return jsonify({
                'status': 404,
                'message': 'data not found'
            })

        elif ('status' not in data):
            return jsonify({
                'status': 404,
                'message': 'status not found in data'
            })
        
        status = data.get('type')

        if (status == 'UNDEFINED' or status == 'ALLOWED' or status == 'BANNED'):
            return jsonify({
                'status': 200,
                'succes': self.dring.config.set_certificate_status(
                            account_id, 
                            cert_id, 
                            status
                          )
            })

        return jsonify({
            'status': 400,
            'message': 'wrong status type'
        })
