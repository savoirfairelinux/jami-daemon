#ifndef CONTACT_DOCK_H
#define CONTACT_DOCK_H

#include <QtGui/QDockWidget>
#include <QSplitter>

#include <akonadi/entitytreeview.h>
#include <akonadi/itemview.h>
#include <klineedit.h>
#include <kabc/addressee.h>
#include <akonadi/contact/contactstreemodel.h>
#include <akonadi/collectioncombobox.h>

class QTreeWidget;
class QTableWidget;
class QComboBox;

class ContactDock : public QDockWidget {
   Q_OBJECT
public:
   ContactDock(QWidget* parent);
   virtual ~ContactDock();
private:
   KLineEdit*                   m_pFilterLE;
   Akonadi::CollectionComboBox* m_pCollCCB;
   QSplitter*                   m_pSplitter;
   QTreeWidget*                 m_pContactView;
   QTableWidget*                m_pCallView;
   QComboBox*                   m_pSortByCBB;

public slots:
   KABC::Addressee::List collectAddressBookContacts() const;
private slots:
   void reloadContact();
};

#endif