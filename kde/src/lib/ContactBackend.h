/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

#ifndef CONTACT_BACKEND_H
#define CONTACT_BACKEND_H

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVariant>

#include "typedefs.h"
#include "Contact.h"

//SFLPhone
class Contact;

//Typedef
typedef QList<Contact*> ContactList;

///@class ContactBackend Allow different way to handle contact without poluting the library
class LIB_EXPORT ContactBackend : public QObject {
   Q_OBJECT
public:
   ContactBackend(QObject* parent);
   virtual ~ContactBackend();
   virtual Contact*    getContactByPhone ( const QString& phoneNumber , bool resolveDNS = false) = 0;
   virtual Contact*    getContactByUid   ( const QString& uid         ) = 0;
   virtual void        editContact       ( Contact*       contact     ) = 0;
   virtual void        addNewContact     ( Contact*       contact     ) = 0;
protected:
   virtual ContactList update_slot       (                     ) = 0;
   QHash<QString,Contact*>        m_ContactByPhone ;
   QHash<QString,Contact*>        m_ContactByUid   ;
public slots:
   ContactList update();
   
private slots:
   
signals:
   
};

#endif