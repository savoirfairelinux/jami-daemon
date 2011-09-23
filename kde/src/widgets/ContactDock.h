#ifndef CONTACT_DOCK_H
#define CONTACT_DOCK_H

#include <QtGui/QDockWidget>
#include <QtGui/QTreeWidget>
#include <QSplitter>

#include <akonadi/entitytreeview.h>
#include <akonadi/itemview.h>
#include <klineedit.h>
#include <kabc/addressee.h>
#include <akonadi/contact/contactstreemodel.h>
#include <akonadi/collectioncombobox.h>

class QListWidget;
class QComboBox;
class QTreeWidgetItem;
class QCheckBox;
class ContactItemWidget;
class ContactTree;

class ContactDock : public QDockWidget {
   Q_OBJECT
public:
   ContactDock(QWidget* parent);
   virtual ~ContactDock();
private:
   //Attributes
   KLineEdit*                   m_pFilterLE;
   QSplitter*                   m_pSplitter;
   ContactTree*                 m_pContactView;
   QListWidget*                 m_pCallView;
   QComboBox*                   m_pSortByCBB;
   QCheckBox*                   m_pShowHistoCK;
   QList<ContactItemWidget*>    m_pContacts;

private slots:
   void reloadContact();
   void loadContactHistory(QTreeWidgetItem* item);
   void filter(QString text);
   void setHistoryVisible(bool visible);
};

class ContactTree : public QTreeWidget {
   Q_OBJECT
public:
   ContactTree(QWidget* parent) : QTreeWidget(parent) {}
   virtual QMimeData* mimeData( const QList<QTreeWidgetItem *> items) const;
   bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
};

#endif