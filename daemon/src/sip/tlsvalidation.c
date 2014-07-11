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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <dirent.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>

#include "logger.h"
#include "tlsvalidation.h"

/**
 * Load the content of a file and return the data pointer to it.
 */
static unsigned char *crypto_file_read(const char *path, size_t *out_len)
{
    struct stat st;
    int fd;
    ssize_t bytes_read;
    size_t file_size;
    unsigned char *data = NULL;

    *out_len = 0;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        ERROR("Failed to open file '%s'.", path);
        return NULL;
    }

    if (fstat(fd, &st) < 0) {
        ERROR("Failed to stat file '%s'.", path);
        goto out;
    }

    if (st.st_size <= 0 || st.st_size > INT_MAX) {
        ERROR("Invalid file '%s' length %ld.", path, st.st_size);
        goto out;
    }

    file_size = st.st_size;
    data = (unsigned char *)malloc(file_size);
    if (!data) {
        ERROR("Not enough memory to read file '%s'.", path);
        goto out;
    }

    do {
        bytes_read = read(fd, &(data[*out_len]), (st.st_size - *out_len));
        if (bytes_read < 0) {
            free(data);
            data = NULL;
            *out_len = 0;
            ERROR("Failed to read file '%s'.", path);
            goto out;
        }
        *out_len += bytes_read;
    } while ((bytes_read > 0) && (*out_len < file_size));

out:
    close(fd);
    return data;
}

/**
 * Check the validity date of a given certificate.
 */
static int crypto_cert_check_date(gnutls_x509_crt_t cert)
{
    time_t now = time(0);
    time_t activationTime, expirationTime;

    activationTime = gnutls_x509_crt_get_activation_time(cert);
    if (activationTime == -1) {
        ERROR("Could not retrieve activation time.");
        return -1;
    }
    if (now < activationTime) {
        ERROR("Certificate not yet activated.");
        return -1;
    }

    expirationTime = gnutls_x509_crt_get_expiration_time(cert);
    if (expirationTime == -1) {
        ERROR("Could not errrieve expiration time.");
        return -2;
    }
    if (now > expirationTime) {
        ERROR("Certificate expired.");
        return -2;
    }

    return 0;
}

/**
 * Load the content of a certificate file and return the data pointer to it.
 */
static unsigned char *crypto_cert_read(const char *path, size_t *out_len)
{
    gnutls_x509_crt_t cert;
    unsigned char *data = NULL;
    gnutls_datum_t dt;
    size_t fsize = 0;
    int err;

    dt.data = crypto_file_read(path, &fsize);
    if (!dt.data)
        return NULL;

    dt.size = (unsigned int) fsize;
    if (gnutls_x509_crt_init(&cert) != GNUTLS_E_SUCCESS) {
        ERROR("Not enough memory for certificate.");
        goto out;
    }

    err = gnutls_x509_crt_import(cert, &dt, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS)
        err = gnutls_x509_crt_import(cert, &dt, GNUTLS_X509_FMT_DER);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not import certificate %s - %s", path, gnutls_strerror(err));
        goto out;
    }

    /* check if cert date is valid */
    err = crypto_cert_check_date(cert);
    if (err < 0)
        goto out;

    *out_len = 10000;
    data = (unsigned char *)malloc(*out_len);
    if (!data)
        goto out;
    err = gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_DER, data, out_len);
    if (err != GNUTLS_E_SUCCESS) {
        free(data);
        data = NULL;
        *out_len = 0;
        ERROR("Certificate %s could not be exported - %s.\n",
              path, gnutls_strerror(err));
    }

out:
    if (dt.data)
        gnutls_free(dt.data);
    gnutls_x509_crt_deinit(cert);
    return data;
}

/**
 * Load all root CAs present in the system.
 * Normally we should use gnutls_certificate_set_x509_system_trust(), but it requires
 * GnuTLS 3.0 or later. As a workaround we iterate on the system trusted store folder
 * and load every certificate available there.
 */
static int crypto_cert_load_trusted(gnutls_certificate_credentials_t cred)
{
    DIR *trust_store;
    struct dirent *trust_ca;
    struct stat statbuf;
    int err, res = -1;
    char ca_file[512];

    trust_store = opendir("/etc/ssl/certs/");
    if (!trust_store) {
        ERROR("Failed to open system trusted store.");
        goto out;
    }
    while ((trust_ca = readdir(trust_store)) != NULL) {
        /* Prepare the string and check it is a regular file. */
        err = snprintf(ca_file, sizeof(ca_file), "/etc/ssl/certs/%s", trust_ca->d_name);
        if (err < 0) {
            ERROR("snprintf() error");
            goto out;
        } else if (err >= sizeof(ca_file)) {
            ERROR("File name too long '%s'.", trust_ca->d_name);
            goto out;
        }
        err = stat(ca_file, &statbuf);
        if (err < 0) {
            ERROR("Failed to stat file '%s'.", ca_file);
            goto out;
        }
        if (!S_ISREG(statbuf.st_mode))
            continue;

        /* Load the root CA. */
        err = gnutls_certificate_set_x509_trust_file(cred, ca_file, GNUTLS_X509_FMT_PEM);
        if (err == 0) {
            WARN("No trusted certificates found - %s", gnutls_strerror(err));
        } else if (err < 0) {
            ERROR("Could not load trusted certificates - %s", gnutls_strerror(err));
            goto out;
        }
    }

    res = 0;
out:
    closedir(trust_store);
    return res;
}

/**
 * Print the Subject, the Issuer and the Verification status of a given certificate.
 */
static int crypto_cert_print_issuer(gnutls_x509_crt_t cert,
                                    gnutls_x509_crt_t issuer)
{
    char name[512];
    char issuer_name[512];
    size_t name_size;
    size_t issuer_name_size;

    issuer_name_size = sizeof(issuer_name);
    gnutls_x509_crt_get_issuer_dn(cert, issuer_name,
                                  &issuer_name_size);

    name_size = sizeof(name);
    gnutls_x509_crt_get_dn(cert, name, &name_size);

    DEBUG("Subject: %s", name);
    DEBUG("Issuer: %s", issuer_name);

    if (issuer != NULL) {
        issuer_name_size = sizeof(issuer_name);
        gnutls_x509_crt_get_dn(issuer, issuer_name, &issuer_name_size);

        DEBUG("Verified against: %s", issuer_name);
    }

    return 0;
}

int containsPrivateKey(const char *pemPath)
{
    gnutls_datum_t dt;
    gnutls_x509_privkey_t key;
    size_t bufsize;
    int err, res = -1;

    dt.data = crypto_file_read(pemPath, &bufsize);
    if (!dt.data)
        return res;
    dt.size = bufsize;

    err = gnutls_global_init();
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init GnuTLS - %s", gnutls_strerror(err));
        free(dt.data);
        return res;
    }

    err = gnutls_x509_privkey_init(&key);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init key - %s", gnutls_strerror(err));
        free(dt.data);
        gnutls_global_deinit();
        return res;
    }

    err = gnutls_x509_privkey_import(key, &dt, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS)
        err = gnutls_x509_privkey_import(key, &dt, GNUTLS_X509_FMT_DER);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not read key - %s", gnutls_strerror(err));
        goto out;
    }

    res = 0;
    DEBUG("Key from %s seems valid.", pemPath);
out:
    free(dt.data);
    gnutls_x509_privkey_deinit(key);
    gnutls_global_deinit();
    return res;
}

int certificateIsValid(const char *caPath, const char *certPath)
{
    gnutls_x509_crt_t ca = NULL;
    gnutls_x509_crt_t cert = NULL;
    gnutls_datum_t ca_dt = {}, cert_dt = {};
    size_t bufsize;
    unsigned int output;
    int err, self_signed;
    int res = -1;

    err = gnutls_global_init();
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init GnuTLS - %s", gnutls_strerror(err));
        goto out;
    }

    cert_dt.data = crypto_cert_read(certPath, &bufsize);
    cert_dt.size = bufsize;
    if (!cert_dt.data)
        goto out;

    err = gnutls_x509_crt_init(&cert);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init certificate - %s", gnutls_strerror(err));
        goto out;
    }

    err = gnutls_x509_crt_import(cert, &cert_dt, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS)
        err = gnutls_x509_crt_import(cert, &cert_dt, GNUTLS_X509_FMT_DER);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not read certificate - %s", gnutls_strerror(err));
        goto out;
    }
    free(cert_dt.data);
    cert_dt.data = NULL;

    /* check if cert is self signed */
    self_signed = gnutls_x509_crt_check_issuer(cert, cert);
    if (!self_signed && !caPath) {
        ERROR("Certificate is not self-signed, and CA is not provided.");
        goto out;
    }
    if (caPath) {
        ca_dt.data = crypto_cert_read(caPath, &bufsize);
        ca_dt.size = bufsize;
        if (!ca_dt.data)
            goto out;

        err = gnutls_x509_crt_init(&ca);
        if (err != GNUTLS_E_SUCCESS) {
            ERROR("Could not init CA - %s", gnutls_strerror(err));
            goto out;
        }

        err = gnutls_x509_crt_import(ca, &ca_dt, GNUTLS_X509_FMT_PEM);
        if (err != GNUTLS_E_SUCCESS)
            err = gnutls_x509_crt_import(ca, &ca_dt, GNUTLS_X509_FMT_DER);
        if (err != GNUTLS_E_SUCCESS) {
            ERROR("Could not read CA - %s", gnutls_strerror(err));
            goto out;
        }
        free(ca_dt.data);
        ca_dt.data = NULL;

        /* Check if the CA is the issuer of certificate. */
        self_signed = gnutls_x509_crt_check_issuer(cert, ca);
        if (!self_signed) {
            ERROR("Certificate is not issued by the provided CA.");
            goto out;
        }

        /* Verify the certificate with its issuer. */
        err = gnutls_x509_crt_verify(cert, &ca, 1, 0, &output);
        if (err < 0) {
            ERROR("Could not verify cert: %s", gnutls_strerror(err));
            goto out;
        }
        if (output & GNUTLS_CERT_INVALID) {
            ERROR("Verification failed.");
            if (output & GNUTLS_CERT_SIGNER_NOT_FOUND)
                ERROR("The certificate hasn't got a known issuer.");
            if (output & GNUTLS_CERT_SIGNER_NOT_CA)
                ERROR("The certificate issuer is not a CA.");
            if (output & GNUTLS_CERT_REVOKED)
                ERROR("The certificate has been revoked.");
            if (output & GNUTLS_CERT_EXPIRED)
                ERROR("The certificate has expired.");
            if (output & GNUTLS_CERT_NOT_ACTIVATED)
                ERROR("The certificate is not yet activated.");
            goto out;
        }
    }

    DEBUG("Certificate from %s seems valid.", certPath);
    crypto_cert_print_issuer(cert, ca);
    res = 0;
out:
    if (ca_dt.data)
        free(ca_dt.data);
    if (cert_dt.data)
        free(cert_dt.data);
    if (ca)
        gnutls_x509_crt_deinit(ca);
    gnutls_x509_crt_deinit(cert);
    gnutls_global_deinit();
    return res;
}

/* mainly based on Fedora Defensive Coding tutorial
 * https://docs.fedoraproject.org/en-US/Fedora_Security_Team/html/Defensive_Coding/sect-Defensive_Coding-TLS-Client-GNUTLS.html
 */
int verifyHostnameCertificate(const char *host, const uint16_t port)
{
    int err, arg, res = -1;
    unsigned int status = (unsigned) -1;
    const char *errptr = NULL;
    gnutls_session_t session = NULL;
    gnutls_certificate_credentials_t cred = NULL;
    unsigned int certslen = 0;
    const gnutls_datum_t *certs = NULL;
    gnutls_x509_crt_t cert = NULL;

    char buf[4096];
    int sockfd;
    struct sockaddr_in name;
    struct hostent *hostinfo;
    const int one = 1;
    fd_set fdset;
    struct timeval tv;

    if (!host || !port) {
        ERROR("Wrong parameters used - host %s, port %d.", host, port);
        return res;
    }

    /* Create the socket. */
    sockfd = socket (PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ERROR("Could not create socket.");
        return res;
    }
    /* Set non-blocking so we can dected timeouts. */
    arg = fcntl(sockfd, F_GETFL, NULL);
    if (arg < 0)
        goto out;
    arg |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, arg) < 0)
        goto out;

    /* Give the socket a name. */
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    hostinfo = gethostbyname(host);
    if (hostinfo == NULL) {
        ERROR("Unknown host %s.", host);
        goto out;
    }
    name.sin_addr = *(struct in_addr *)hostinfo->h_addr;
    /* Connect to the address specified in name struct. */
    err = connect(sockfd, (struct sockaddr *)&name, sizeof(name));
    if (err < 0) {
        /* Connection in progress, use select to see if timeout is reached. */
        if (errno == EINPROGRESS) {
            do {
                FD_ZERO(&fdset);
                FD_SET(sockfd, &fdset);
                tv.tv_sec = 10;     // 10 second timeout
                tv.tv_usec = 0;
                err = select(sockfd + 1, NULL, &fdset, NULL, &tv);
                if (err < 0 && errno != EINTR) {
                    ERROR("Could not connect to hostname %s at port %d",
                          host, port);
                    goto out;
                } else if (err > 0) {
                    /* Select returned, if so_error is clean we are ready. */
                    int so_error;
                    socklen_t len = sizeof(so_error);
                    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

                    if (so_error) {
                        ERROR("Connection delayed.");
                        goto out;
                    }
                    break;  // exit do-while loop
                } else {
                    ERROR("Connection timeout.");
                    goto out;
                }
            } while(1);
        } else {
            ERROR("Could not connect to hostname %s at port %d", host, port);
            goto out;
        }
    }
    /* Set the socked blocking again. */
    arg = fcntl(sockfd, F_GETFL, NULL);
    if (arg < 0)
        goto out;
    arg &= ~O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, arg) < 0)
        goto out;

    /* Disable Nagle algorithm that slows down the SSL handshake. */
    err = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    if (err < 0) {
        ERROR("Could not set TCP_NODELAY.");
        goto out;
    }

    err = gnutls_global_init();
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init GnuTLS - %s", gnutls_strerror(err));
        goto out;
    }
    /* Load the trusted CA certificates. */
    err = gnutls_certificate_allocate_credentials(&cred);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not allocate credentials - %s", gnutls_strerror(err));
        goto out;
    }
    err = crypto_cert_load_trusted(cred);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not load credentials.");
        goto out;
    }

    /* Create the session object. */
    err = gnutls_init(&session, GNUTLS_CLIENT);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init session -%s\n", gnutls_strerror(err));
        goto out;
    }

    /* Configure the cipher preferences. The default set should be good enough. */
    err = gnutls_priority_set_direct(session, "NORMAL", &errptr);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not set up ciphers - %s (%s)", gnutls_strerror(err), errptr);
        goto out;
    }

    /* Install the trusted certificates. */
    err = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, cred);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not set up credentials - %s", gnutls_strerror(err));
        goto out;
    }

    /* Associate the socket with the session object and set the server name. */
    gnutls_transport_set_ptr(session, (gnutls_transport_ptr_t) (uintptr_t) sockfd);
    err = gnutls_server_name_set(session, GNUTLS_NAME_DNS, host, strlen(host));
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not set server name - %s", gnutls_strerror(err));
        goto out;
    }

    /* Establish the connection. */
    err = gnutls_handshake(session);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Handshake failed - %s", gnutls_strerror(err));
        goto out;
    }
    /* Obtain the server certificate chain. The server certificate
     * itself is stored in the first element of the array. */
    certs = gnutls_certificate_get_peers(session, &certslen);
    if (certs == NULL || certslen == 0) {
        ERROR("Could not obtain peer certificate - %s", gnutls_strerror(err));
        goto out;
    }

    /* Validate the certificate chain. */
    err = gnutls_certificate_verify_peers2(session, &status);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not verify the certificate chain - %s", gnutls_strerror(err));
        goto out;
    }
    if (status != 0) {
        gnutls_datum_t msg;
#if GNUTLS_VERSION_AT_LEAST_3_1_4
        int type = gnutls_certificate_type_get(session);
        err = gnutls_certificate_verification_status_print(status, type, &out, 0);
#else
        err = -1;
#endif
        if (err == 0) {
            ERROR("Certificate validation failed - %s\n", msg.data);
            gnutls_free(msg.data);
            goto out;
        } else {
            ERROR("Certificate validation failed with code 0x%x.", status);
            goto out;
        }
    }

    /* Match the peer certificate against the hostname.
     * We can only obtain a set of DER-encoded certificates from the
     * session object, so we have to re-parse the peer certificate into
     * a certificate object. */

    err = gnutls_x509_crt_init(&cert);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init certificate - %s", gnutls_strerror(err));
        goto out;
    }

    /* The peer certificate is the first certificate in the list. */
    err = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS)
        err = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_DER);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not read peer certificate - %s", gnutls_strerror(err));
        goto out;
    }
    /* Finally check if the hostnames match. */
    err = gnutls_x509_crt_check_hostname(cert, host);
    if (err == 0) {
        ERROR("Hostname %s does not match certificate.", host);
        goto out;
    }

    /* Try sending and receiving some data through. */
    snprintf(buf, sizeof(buf), "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", host);
    err = gnutls_record_send(session, buf, strlen(buf));
    if (err < 0) {
        ERROR("Send failed - %s", gnutls_strerror(err));
        goto out;
    }
    err = gnutls_record_recv(session, buf, sizeof(buf));
    if (err < 0) {
        ERROR("Recv failed - %s", gnutls_strerror(err));
        goto out;
    }

    DEBUG("Hostname %s seems to point to a valid server.", host);
    res = 0;
out:
    if (session) {
        gnutls_bye(session, GNUTLS_SHUT_RDWR);
        gnutls_deinit(session);
    }
    if (cert)
        gnutls_x509_crt_deinit(cert);
    if (cred)
        gnutls_certificate_free_credentials(cred);
    gnutls_global_deinit();
    close(sockfd);
    return res;
}
