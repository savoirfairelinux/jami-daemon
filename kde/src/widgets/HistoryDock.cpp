#include "HistoryDock.h"

#include <QtGui/QVBoxLayout>
#include <kicon.h>
#include <klineedit.h>
#include <QtGui/QTreeWidget>
#include <QtGui/QComboBox>
#include <QtGui/QLabel>
#include <kdatewidget.h>
#include <QtGui/QCheckBox>
#include "conf/ConfigurationSkeleton.h"

HistoryDock::HistoryDock(QWidget* parent) : QDockWidget(parent)
{
   m_pFilterLE   = new KLineEdit();
   m_pItemView   = new QTreeWidget(this);
   m_pSortByCBB  = new QComboBox();
   m_pSortByL    = new QLabel("Sort by:");
   m_pFromL      = new QLabel("From:");
   m_pToL        = new QLabel("To:");
   m_pFromDW     = new KDateWidget();
   m_pToDW       = new KDateWidget();
   m_pAllTimeCB  = new QCheckBox("Display all");
   
   m_pAllTimeCB->setChecked(ConfigurationSkeleton::displayDataRange());
   
   m_pSortByCBB->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
   
   QStringList sortBy;
   sortBy << "Date" << "Name" << "Popularity";
   m_pSortByCBB->addItems(sortBy);

   QWidget* mainWidget = new QWidget(this);
   setWidget(mainWidget);

   QGridLayout* mainLayout = new QGridLayout(mainWidget);

   mainLayout->addWidget(m_pSortByL   ,0,0     );
   mainLayout->addWidget(m_pSortByCBB ,0,1     );
   
   mainLayout->addWidget(m_pAllTimeCB ,1,0,1,2 );
   mainLayout->addWidget(m_pFromL     ,2,0,1,2 );
   mainLayout->addWidget(m_pFromDW    ,3,0,1,2 );
   mainLayout->addWidget(m_pToL       ,4,0,1,2 );
   mainLayout->addWidget(m_pToDW      ,5,0,1,2 );
   
   mainLayout->addWidget(m_pItemView  ,6,0,1,2 );
   mainLayout->addWidget(m_pFilterLE  ,7,0,1,2 );
   
   setWindowTitle("History");
   
   connect(m_pAllTimeCB,SIGNAL(toggled(bool)),this,SLOT(enableDateRange(bool)));
}

HistoryDock::~HistoryDock()
{
}

void HistoryDock::enableDateRange(bool enable)
{
   m_pFromL->setVisible(enable);
   m_pToL->setVisible(enable);
   m_pFromDW->setVisible(enable);
   m_pToDW->setVisible(enable);
   
   ConfigurationSkeleton::setDisplayDataRange(enable);
}