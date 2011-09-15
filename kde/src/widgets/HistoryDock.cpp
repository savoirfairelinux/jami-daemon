#include "HistoryDock.h"

#include <QtGui/QVBoxLayout>
#include <kicon.h>
#include <klineedit.h>
#include <QtGui/QTreeWidget>
#include <QtGui/QComboBox>
#include <QtGui/QLabel>


HistoryDock::HistoryDock(QWidget* parent) : QDockWidget(parent)
{
   m_pFilterLE   = new KLineEdit();
   m_pItemView   = new QTreeWidget(this);
   m_pSortByCBB  = new QComboBox();
   m_pSortByL    = new QLabel();
   
   m_pSortByL->setText("Sort by:");
   m_pSortByCBB->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);

   QWidget* mainWidget = new QWidget(this);
   setWidget(mainWidget);

   QGridLayout* mainLayout = new QGridLayout(mainWidget);

   mainLayout->addWidget(m_pSortByL   ,0,0     );
   mainLayout->addWidget(m_pSortByCBB ,0,1     );
   mainLayout->addWidget(m_pItemView  ,1,0,1,2 );
   mainLayout->addWidget(m_pFilterLE  ,2,0,1,2 );
   
   setWindowTitle("History");
}

HistoryDock::~HistoryDock()
{
   
}
