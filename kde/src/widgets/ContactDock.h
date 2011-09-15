#ifndef CONTACT_DOCK_H
#define CONTACT_DOCK_H

#include <QtGui/QDockWidget>
#include <QSplitter>

#include <akonadi/entitytreeview.h>
#include <akonadi/itemview.h>
//#include <akonadi/contactgroupviewer.h>
#include <klineedit.h>
#include <kabc/addressee.h>
#include <akonadi/contact/contactstreemodel.h>
#include <akonadi/collectioncombobox.h>


class ContactDock : public QDockWidget {
   Q_OBJECT
public:
   ContactDock(QWidget* parent);
   virtual ~ContactDock();
private:
   KABC::Addressee::List collectAllContacts(Akonadi::ContactsTreeModel *mModel) const;
   Akonadi::EntityTreeView* m_pCollViewCV;
   Akonadi::ItemView*       m_pItemView;
   KLineEdit*               m_pFilterLE;
   Akonadi::CollectionComboBox* m_pCollCCB;
   QSplitter* m_pSplitter;

public slots:
   KABC::Addressee::List collectAddressBookContacts() const;
};

#endif