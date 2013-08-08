/*
 *  Copyright (C) 2012б 2013 LOTES TM LLC
 *  Author : Andrey Loukhnov <aol.nnov@gmail.com>
 *
 *  This file is a part of pult5-voip
 *
 *  pult5-voip is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  pult5-voip is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this programm. If not, see <http://www.gnu.org/licenses/>.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify pult5-voip, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, LOTES-TM LLC
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef SIPVOIP_PRES_H
#define	SIPVOIP_PRES_H
#include "pjsip/sip_types.h"
#include "pjsip/sip_module.h"
#include "pjsip/sip_msg.h"
#include "pjsip/sip_multipart.h"

//PJ_BEGIN_DECL


extern pj_bool_t pres_on_rx_subscribe_request(pjsip_rx_data *rdata);

static pjsip_module my_mod_pres = {
    NULL, NULL, /* prev, next.		*/
    { "mod-lotes-presence", 18}, /* Name.		*/
    -1, /* Id			*/
    //        PJSIP_MOD_PRIORITY_APPLICATION, /* Priority	        */
    PJSIP_MOD_PRIORITY_DIALOG_USAGE,
    NULL, /* load()		*/
    NULL, /* start()		*/
    NULL, /* stop()		*/
    NULL, /* unload()		*/
    &pres_on_rx_subscribe_request, /* on_rx_request()	*/
    NULL, /* on_rx_response()	*/
    NULL, /* on_tx_request.	*/
    NULL, /* on_tx_response()	*/
    NULL, /* on_tsx_state()	*/

};

struct pres_msg_data
{
    /**
     * Additional message headers as linked list. Application can add
     * headers to the list by creating the header, either from the heap/pool
     * or from temporary local variable, and add the header using
     * linked list operation. See pjsip_apps.c for some sample codes.
     */
    pjsip_hdr	hdr_list;

    /**
     * MIME type of optional message body.
     */
    pj_str_t	content_type;

    /**
     * Optional message body to be added to the message, only when the
     * message doesn't have a body.
     */
    pj_str_t	msg_body;

    /**
     * Content type of the multipart body. If application wants to send
     * multipart message bodies, it puts the parts in \a parts and set
     * the content type in \a multipart_ctype. If the message already
     * contains a body, the body will be added to the multipart bodies.
     */
    pjsip_media_type  multipart_ctype;

    /**
     * List of multipart parts. If application wants to send multipart
     * message bodies, it puts the parts in \a parts and set the content
     * type in \a multipart_ctype. If the message already contains a body,
     * the body will be added to the multipart bodies.
     */
    pjsip_multipart_part multipart_parts;
};

//pjsip_pres_status pres_status_data;
pjsip_pres_status *  pres_get_data();
extern void pres_update(const std::string &status, const std::string &note);
extern void pres_process_msg_data(pjsip_tx_data *tdata, const pres_msg_data *msg_data);

//PJ_END_DECL
#endif	/* SIPVOIP_PRES_H */
