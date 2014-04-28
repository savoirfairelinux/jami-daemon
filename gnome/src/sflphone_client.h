/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifndef SFLPHONE_CLIENT_H_
#define SFLPHONE_CLIENT_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#define SFLPHONE_GSETTINGS_SCHEMA "org.sflphone.SFLphone"
#define SFLPHONE_TYPE_CLIENT (sflphone_client_get_type())
#define SFLPHONE_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SFLPHONE_TYPE_CLIENT, SFLPhoneClient))

typedef struct
{
    GtkApplication parent;
    /* TODO: hide implementation */
    GSettings *settings;
    /* Main window */
    GtkWidget *win;
    /* Main toolbar */
    GtkWidget *toolbar;
#ifdef SFL_VIDEO
    /* Video window */
    GtkWidget *video;
#endif

} SFLPhoneClient;

typedef GtkApplicationClass SFLPhoneClientClass;

SFLPhoneClient *
sflphone_client_new();

#endif
