#ifndef CONTACT_DOCK_H
#define CONTACT_DOCK_H

#include <QtGui/QDockWidget>
#include <QtGui/QTreeWidget>

//Qt
class QSplitter;
class QListWidget;
class QComboBox;
class QTreeWidgetItem;
class QCheckBox;

//KDE
class KLineEdit;

namespace Akonadi {
   class EntityTreeView;
   class ItemView;
   class CollectionCombobox;
   namespace Contact {
      class ContactsTreeModel;
   }
}

namespace KABC {
   class Addressee;
}

///SFLPhone
class ContactTree;
class ContactItemWidget;

class ContactDock : public QDockWidget
{
   Q_OBJECT
public:
   //Constructor
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
   
public slots:
   virtual void keyPressEvent(QKeyEvent* event);
   
private slots:
   void reloadContact();
   void loadContactHistory(QTreeWidgetItem* item);
   void filter(QString text);
   void setHistoryVisible(bool visible);
};

///@class ContactTree tree view with additinal drag and drop
class ContactTree : public QTreeWidget {
   Q_OBJECT
public:
   ContactTree(QWidget* parent) : QTreeWidget(parent) {}
   virtual QMimeData* mimeData( const QList<QTreeWidgetItem *> items) const;
   bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
};

///@class KeyPressEaterC keygrabber
class KeyPressEaterC : public QObject
{
   Q_OBJECT
public:
   KeyPressEaterC(ContactDock* parent) : QObject(parent) {
      m_pDock =  parent;
   }
   
protected:
   bool eventFilter(QObject *obj, QEvent *event);
   
private:
   ContactDock* m_pDock;
   
};

#endif