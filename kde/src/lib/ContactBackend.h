/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef CONTACT_BACKEND_H
#define CONTACT_BACKEND_H

#include <QObject>
#include <QHash>

#include "typedefs.h"

//SFLPhone
class Contact;

typedef QList<Contact*> ContactList;

class LIB_EXPORT ContactBackend : public QObject {
   Q_OBJECT
public:
   ContactBackend(QObject* parent);
   virtual Contact*    getContactByPhone ( QString phoneNumber ) = 0;
   virtual Contact*    getContactByUid   ( QString uid         ) = 0;
   virtual void        editContact       ( Contact* contact    ) = 0;
   virtual void        addNewContact     ( Contact* contact    ) = 0;
protected:
   virtual ContactList update_slot       (                     ) = 0;
   QHash<QString,Contact*>        m_pContactByPhone ;
   QHash<QString,Contact*>        m_pContactByUid   ;
public slots:
   ContactList update();
   
private slots:
   
signals:
   
};

#endif