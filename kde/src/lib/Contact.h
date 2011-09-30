/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
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
 **************************************************************************/
#ifndef CONTACT_H
#define CONTACT_H

#include <QObject>

//Qt
class QListWidgetItem;
class QWidget;
class QPixmap;

//KDE
namespace KABC {
   class Addressee   ;
   class Picture     ;
   class PhoneNumber ;
}

#include "typedefs.h"

/**
   @author Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>
   @author Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
*/
class LIB_EXPORT Contact : public QObject{
   Q_OBJECT
public:
   class PhoneNumber {
   public:
      PhoneNumber(QString number, QString type)
      : m_pNumber(number),m_pType(type){}
      QString& getNumber() {
         return m_pNumber ;
      }
      QString& getType() {
         return m_pType   ;
      }
   private:
      QString m_pNumber   ;
      QString m_pType     ;
   };
   typedef QList<Contact::PhoneNumber*> PhoneNumbers;
   
private:
   QString      m_pFirstName      ;
   QString      m_pSecondName     ;
   QString      m_pNickName       ;
   QPixmap*     m_pPhoto          ;
   QString      m_pType           ;
   QString      m_pFormattedName  ;
   QString      m_pPreferredEmail ;
   QString      m_pOrganization   ;
   QString      m_pUid            ;
   bool         displayPhoto      ;
   PhoneNumbers m_pNumbers        ;
public:
   //Constructors & Destructors
   explicit Contact();
   virtual ~Contact();
   
   //Getters
   virtual PhoneNumbers   getPhoneNumbers()    const;
   virtual QString        getNickName()        const;
   virtual QString        getFirstName()       const;
   virtual QString        getSecondName()      const;
   virtual QString        getFormattedName()   const;
   virtual QString        getOrganization()    const;
   virtual QString        getUid()             const;
   virtual QString        getPreferredEmail()  const;
   virtual const QPixmap* getPhoto()           const;
   virtual QString        getType()            const;
   virtual void           initItem();

   //Setters
   virtual void setPhoneNumbers   (PhoneNumbers   );
   virtual void setFormattedName  (QString name   );
   virtual void setNickName       (QString name   );
   virtual void setFirstName      (QString name   );
   virtual void setFamilyName     (QString name   );
   virtual void setOrganization   (QString name   );
   virtual void setPreferredEmail (QString name   );
   virtual void setUid            (QString id     );
   virtual void setPhoto          (QPixmap* photo );
   
protected:
   virtual void initItemWidget();

};
typedef Contact::PhoneNumbers PhoneNumbers;

#endif
