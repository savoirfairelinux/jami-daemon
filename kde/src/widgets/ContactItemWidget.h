/***************************************************************************
 *   Author : Mathieu Leduc-Hamel mathieu.leduc-hamel@savoirfairelinux.com *
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

#ifndef CONTACTITEMWIDGET_H
#define CONTACTITEMWIDGET_H

#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#include <QtGui/QWidget>
#include <QtGui/QLabel>
#include <QtGui/QSpacerItem>
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>
#include <KIcon>
#include <kabc/addressee.h>
#include <kabc/picture.h>
#include <kabc/phonenumber.h>

#include <lib/Contact.h>

class QTreeWidgetItem;
class KAction;
class QMenu;

class ContactItemWidget : public QWidget
{
   Q_OBJECT
 public:
    ContactItemWidget(QWidget* parent =0);
    ~ContactItemWidget();

    KAction* m_pCallAgain;
    KAction* m_pEditContact;
    KAction* m_pCopy;
    KAction* m_pEmail;
    KAction* m_pAddPhone;
    QMenu*   m_pMenu;

    KABC::Addressee* contact() const;
    void setContact(Contact* contact);
    static const char * callStateIcons[12];

    //QPixmap* getIcon();
    QString  getContactName() const;
    PhoneNumbers getCallNumbers() const;
    QString  getOrganization() const;
    QString  getEmail() const;
    QPixmap* getPicture() const;
    QTreeWidgetItem* getItem();
    Contact* getContact();

    void setItem(QTreeWidgetItem* item);

 private:
    Contact* m_pContactKA;

    QLabel* m_pIconL;
    QLabel* m_pContactNameL;
    QLabel* m_pCallNumberL;
    QLabel* m_pOrganizationL;
    QLabel* m_pEmailL;

    QTreeWidgetItem* m_pItem;

    bool init;

public slots:
   void updated();
private slots:
   void showContext(const QPoint& pos);
   void sendEmail();
   void callAgain();
   void copy();
   void editContact();
   void addPhone();
 };

#endif // CALLTREE_ITEM_H
