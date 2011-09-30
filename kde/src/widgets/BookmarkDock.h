#ifndef BOOKMARK_DOCK_H
#define BOOKMARK_DOCK_H

#include <QtGui/QDockWidget>

//Qt
class QTreeWidget;
class QSplitter;

//KDE
class KLineEdit;

//SFLPhonr
class HistoryTreeItem;

typedef QList<HistoryTreeItem*> BookmarkList;
class BookmarkDock : public QDockWidget {
   Q_OBJECT
public:
   BookmarkDock(QWidget* parent);
   virtual ~BookmarkDock();
   void addBookmark(QString phone);
private:
   QTreeWidget*  m_pItemView;
   KLineEdit*    m_pFilterLE;
   QSplitter*    m_pSplitter;
   BookmarkList  m_pBookmark;
   void addBookmark_internal(QString phone);
private slots:
   void filter(QString text);
};

#endif