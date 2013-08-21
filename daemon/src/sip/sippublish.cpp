/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
*
*  Author: Patrick Keroulas <patrick.keroulas@savoirfairelinux.com>
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

#include <pjsip/sip_endpoint.h>

#include "sippresence.h"
#include "sip_utils.h"
#include "sippublish.h"
#include "logger.h"
#include "sipvoiplink.h"

void pres_publish_cb(struct pjsip_publishc_cbparam *param);
pj_status_t pres_send_publish(SIPPresence * pres, pj_bool_t active);
const pjsip_publishc_opt  my_publish_opt = {true}; // this is queue_request


/*
 * Client presence publication callback.
 */
void pres_publish_cb(struct pjsip_publishc_cbparam *param)
{
    SIPPresence *pres = (SIPPresence*) param->token;

    if (param->code/100 != 2 || param->status != PJ_SUCCESS) {

	pjsip_publishc_destroy(param->pubc);
	pres->publish_sess = NULL;

	if (param->status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    pj_strerror(param->status, errmsg, sizeof(errmsg));
	    ERROR("Client publication (PUBLISH) failed, status=%d, msg=%s", param->status, errmsg);
	} else if (param->code == 412) {
	    /* 412 (Conditional Request Failed)
	     * The PUBLISH refresh has failed, retry with new one.
	     */
            WARN("Publish retry.");
	    pres_publish(pres);
	} else {
	    ERROR("Client publication (PUBLISH) failed (%d/%.*s)",
                    param->code,(int)param->reason.slen,param->reason.ptr);
	}

    } else {
	if (param->expiration < 1) {
	    /* Could happen if server "forgot" to include Expires header
	     * in the response. We will not renew, so destroy the pubc.
	     */
	    pjsip_publishc_destroy(param->pubc);
	    pres->publish_sess = NULL;
	}
    }
}

/*
 * Send PUBLISH request.
 */
pj_status_t pres_send_publish(SIPPresence * pres, pj_bool_t active)
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    DEBUG("Send presence %sPUBLISH..", (active ? "" : "un-"));

    SIPAccount * acc = pres->getAccount();
    std::string contactWithAngles =  acc->getFromUri();
    contactWithAngles.erase(contactWithAngles.find('>'));
    int semicolon = contactWithAngles.find_first_of(":");
    std::string contactWithoutAngles = contactWithAngles.substr(semicolon + 1);
//    pj_str_t contact = pj_str(strdup(contactWithoutAngles.c_str()));
//    pj_memcpy(&pres_status_data.info[0].contact, &contt, sizeof(pj_str_t));;

    /* Create PUBLISH request */
    if (active) {
	char *bpos;
	pj_str_t entity;

	status = pjsip_publishc_publish(pres->publish_sess, PJ_TRUE, &tdata);
	if (status != PJ_SUCCESS) {
	    ERROR("Error creating PUBLISH request", status);
	    goto on_error;
	}

        pj_str_t from = pj_str(strdup(acc->getFromUri().c_str()));
	if ((bpos=pj_strchr(&from, '<')) != NULL) {
	    char *epos = pj_strchr(&from, '>');
	    if (epos - bpos < 2) {
		pj_assert(!"Unexpected invalid URI");
		status = PJSIP_EINVALIDURI;
		goto on_error;
	    }
	    entity.ptr = bpos+1;
	    entity.slen = epos - bpos - 1;
	} else {
	    entity = from;
	}

	/* Create and add PIDF message body */
	status = pjsip_pres_create_pidf(tdata->pool, acc->getPresence()->getStatus(),
                &entity, &tdata->msg->body);
	if (status != PJ_SUCCESS) {
	    ERROR("Error creating PIDF for PUBLISH request");
	    pjsip_tx_data_dec_ref(tdata);
	    goto on_error;
	}

    } else {
	//status = pjsip_publishc_unpublish(pres->publish_sess, &tdata);
	//if (status != PJ_SUCCESS) {
	//    pjsua_perror(THIS_FILE, "Error creating PUBLISH request", status);
	//    goto on_error;
	//}
        WARN("SHOULD UNPUBLISH");
    }


    pres_msg_data msg_data;
    pj_bzero(&msg_data, sizeof(msg_data));
    pj_list_init(&msg_data.hdr_list);
    pjsip_media_type_init(&msg_data.multipart_ctype, NULL, NULL);
    pj_list_init(&msg_data.multipart_parts);

    pres->fillDoc(tdata, &msg_data);


    /* Set Via sent-by */
    /*if (acc->cfg.allow_via_rewrite && acc->via_addr.host.slen > 0) {
        pjsip_publishc_set_via_sent_by(acc->publish_sess, &acc->via_addr,
                                       acc->via_tp);
    } else if (!pjsua_sip_acc_is_using_stun(acc_id)) {
	// Choose local interface to use in Via if acc is not using STUN. See https://trac.pjsip.org/repos/ticket/1412
	pjsip_host_port via_addr;
	const void *via_tp;

	if (pjsua_acc_get_uac_addr(acc_id, acc->pool, &acc_cfg->id,
				   &via_addr, NULL, NULL,
				   &via_tp) == PJ_SUCCESS)
        {
	    pjsip_publishc_set_via_sent_by(acc->publish_sess, &via_addr,
	                                   (pjsip_transport*)via_tp);
        }
    }*/

    /* Send the PUBLISH request */
    status = pjsip_publishc_send(pres->publish_sess, tdata);
    if (status == PJ_EPENDING) {
	WARN("Previous request is in progress, ");
    } else if (status != PJ_SUCCESS) {
	ERROR("Error sending PUBLISH request");
	goto on_error;
    }

    pres->publish_state = pres->online_status;
    return PJ_SUCCESS;

on_error:
    if (pres->publish_sess) {
	pjsip_publishc_destroy(pres->publish_sess);
	pres->publish_sess = NULL;
    }
    return status;
}


/* Create client publish session */
pj_status_t pres_publish(SIPPresence *pres)
{
    pj_status_t status;
    const pj_str_t STR_PRESENCE = pj_str("presence");
    SIPAccount * acc = pres->getAccount();
    pjsip_endpoint *endpt = ((SIPVoIPLink*) acc->getVoIPLink())->getEndpoint();

    /* Create and init client publication session */
    if (pres->publish_enabled) {

	/* Create client publication */
	status = pjsip_publishc_create(endpt,&my_publish_opt,
				       pres, &pres_publish_cb,
				       &pres->publish_sess);
	if (status != PJ_SUCCESS) {
	    pres->publish_sess = NULL;
            ERROR("Failed to create a publish seesion.");
            return status;
	}

	/* Initialize client publication */
        pj_str_t from = pj_str(strdup(acc->getFromUri().c_str()));
	status = pjsip_publishc_init(pres->publish_sess, &STR_PRESENCE,&from, &from, &from, 0xFFFF);
	if (status != PJ_SUCCESS) {
            ERROR("Failed to init a publish session");
	    pres->publish_sess = NULL;
	    return status;
	}

	/* Add credential for authentication */
        if (acc->hasCredentials() and pjsip_publishc_set_credentials(pres->publish_sess, acc->getCredentialCount(), acc->getCredInfo()) != PJ_SUCCESS) {
            ERROR("Could not initialize credentials for invite session authentication");
            return status;
        }

	/* Set route-set */
        if (acc->hasServiceRoute())
            pjsip_regc_set_route_set(acc->getRegistrationInfo(), sip_utils::createRouteSet(acc->getServiceRoute(), pres->getPool()));

	/* Send initial PUBLISH request */
        status = pres_send_publish(pres, PJ_TRUE);
        if (status != PJ_SUCCESS)
            return status;

    } else {
        pres->publish_sess = NULL;
    }

    return PJ_SUCCESS;
}
