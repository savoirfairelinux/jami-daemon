/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "logger.h"

#include "recordable.h"

namespace ring {

/*
 * @file selfierecorder.h
 * @brief Class for recording messages locally
 */


/*
 * The SelfieRecorder class aimes to expose the Recordable interface for
 * recording messages locally. TODO: Explain why empty ?
 */

class SelfieRecorder : public Recordable {
    protected:

        /**
         * Constructor of a SelfieRecorder
         */
        Call(); // TODO
};

} // namespace ring
