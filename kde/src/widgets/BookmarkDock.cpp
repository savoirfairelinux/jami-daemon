#include "BookmarkDock.h"

#include <QtGui/QVBoxLayout>
#include <kicon.h>
#include <klineedit.h>
#include <QtGui/QTreeWidget>
#include <QtGui/QSplitter>

BookmarkDock::BookmarkDock(QWidget* parent) : QDockWidget(parent)
{
   setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
   setMinimumSize(250,0);
   
   m_pFilterLE   = new KLineEdit(this);
   m_pSplitter   = new QSplitter(Qt::Vertical,this);
   m_pItemView   = new QTreeWidget(this);

   QWidget* mainWidget = new QWidget(this);
   setWidget(mainWidget);

   QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

   mainLayout->addWidget(m_pSplitter);
   m_pSplitter->addWidget(m_pItemView);
   mainLayout->addWidget(m_pFilterLE);
   
   m_pSplitter->setChildrenCollapsible(true);
   m_pSplitter->setStretchFactor(0,7);
   
   setWindowTitle("Bookmark");
}

BookmarkDock::~BookmarkDock()
{
   
}
