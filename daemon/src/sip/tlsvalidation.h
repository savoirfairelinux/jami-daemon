/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
 *          Vittorio Giovara <vittorio.giovara@savoirfairelinux.com>
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

#ifndef SECURITY_EVALUATOR_H
#define SECURITY_EVALUATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

    /**
     * Check if the given .pem contains a valid private key.
     *
     * @return 0 if success, -1 otherwise
     */
    int containsPrivateKey(const char *pemPath);

    /**
     * Check if the given .pem contains a valid certificate.
     *
     * @return 0 if success, -1 otherwise
     */
    int certificateIsValid(const char *caPath,
                           const char *pemPath);

    /**
     * Verify that the local hostname points to a valid SSL server by
     * establishing a connection to it and by validating its certificate.
     *
     * @param host the DNS domain address that the certificate should feature
     * @return 0 if success, -1 otherwise
     */
    int verifyHostnameCertificate(const char *host,
                                  const uint16_t port);

#ifdef __cplusplus
}
#endif

#endif
