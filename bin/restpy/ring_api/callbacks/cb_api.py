#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Author: Seva Ivanov <seva.ivanov@savoirfairelinux.com>
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

# This file is the python callbacks API.
# It is written to help the user to identify the python callbacks to register.

# Function names should be the same as the keys from callbacks_to_register().
# Each method should contain a docstring that describes it.

def account_message(account_id, from_ring_id, content):
    """Receive account message

    Keyword arguments:
    account_id      -- account id string
    from_ring_id    -- ring id string
    content         -- dict of content defined as [<mime-type>, <message>]
    """
    pass
