#include <kdialog.h>
#include <klocale.h>

/********************************************************************************
** Form generated from reading UI file 'dlgaddressbookbase.ui'
**
** Created: Tue Apr 20 14:19:42 2010
**      by: Qt User Interface Compiler version 4.6.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DLGADDRESSBOOKBASE_H
#define UI_DLGADDRESSBOOKBASE_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QGroupBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QSlider>
#include <QtGui/QSpacerItem>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>
#include "knuminput.h"

QT_BEGIN_NAMESPACE

class Ui_DlgAddressBookBase
{
public:
    QVBoxLayout *verticalLayout;
    QCheckBox *kcfg_enableAddressBook;
    QWidget *widget_configAddressBookGeneral;
    QVBoxLayout *verticalLayout_2;
    QWidget *widget_maxResults;
    QHBoxLayout *horizontalLayout_4;
    QLabel *label_maxResults;
    QSlider *horizontalSlider_maxResults;
    KIntSpinBox *kcfg_maxResults;
    QCheckBox *kcfg_displayPhoto;
    QGroupBox *groupBox_displayTypes;
    QHBoxLayout *horizontalLayout_7;
    QCheckBox *kcfg_business;
    QCheckBox *kcfg_mobile;
    QCheckBox *kcfg_home;
    QSpacerItem *verticalSpacer_configAddressBook;

    void setupUi(QWidget *DlgAddressBookBase)
    {
        if (DlgAddressBookBase->objectName().isEmpty())
            DlgAddressBookBase->setObjectName(QString::fromUtf8("DlgAddressBookBase"));
        DlgAddressBookBase->resize(350, 250);
        DlgAddressBookBase->setWindowTitle(QString::fromUtf8("Form"));
        verticalLayout = new QVBoxLayout(DlgAddressBookBase);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        kcfg_enableAddressBook = new QCheckBox(DlgAddressBookBase);
        kcfg_enableAddressBook->setObjectName(QString::fromUtf8("kcfg_enableAddressBook"));

        verticalLayout->addWidget(kcfg_enableAddressBook);

        widget_configAddressBookGeneral = new QWidget(DlgAddressBookBase);
        widget_configAddressBookGeneral->setObjectName(QString::fromUtf8("widget_configAddressBookGeneral"));
        widget_configAddressBookGeneral->setEnabled(false);
        verticalLayout_2 = new QVBoxLayout(widget_configAddressBookGeneral);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, -1, -1, -1);
        widget_maxResults = new QWidget(widget_configAddressBookGeneral);
        widget_maxResults->setObjectName(QString::fromUtf8("widget_maxResults"));
        horizontalLayout_4 = new QHBoxLayout(widget_maxResults);
#ifndef UI_Q_OS_MAC
        horizontalLayout_4->setSpacing(-1);
#endif
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        horizontalLayout_4->setContentsMargins(0, 5, 5, 5);
        label_maxResults = new QLabel(widget_maxResults);
        label_maxResults->setObjectName(QString::fromUtf8("label_maxResults"));

        horizontalLayout_4->addWidget(label_maxResults);

        horizontalSlider_maxResults = new QSlider(widget_maxResults);
        horizontalSlider_maxResults->setObjectName(QString::fromUtf8("horizontalSlider_maxResults"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(horizontalSlider_maxResults->sizePolicy().hasHeightForWidth());
        horizontalSlider_maxResults->setSizePolicy(sizePolicy);
        horizontalSlider_maxResults->setMinimum(25);
        horizontalSlider_maxResults->setMaximum(50);
        horizontalSlider_maxResults->setOrientation(Qt::Horizontal);

        horizontalLayout_4->addWidget(horizontalSlider_maxResults);

        kcfg_maxResults = new KIntSpinBox(widget_maxResults);
        kcfg_maxResults->setObjectName(QString::fromUtf8("kcfg_maxResults"));

        horizontalLayout_4->addWidget(kcfg_maxResults);


        verticalLayout_2->addWidget(widget_maxResults);

        kcfg_displayPhoto = new QCheckBox(widget_configAddressBookGeneral);
        kcfg_displayPhoto->setObjectName(QString::fromUtf8("kcfg_displayPhoto"));

        verticalLayout_2->addWidget(kcfg_displayPhoto);

        groupBox_displayTypes = new QGroupBox(widget_configAddressBookGeneral);
        groupBox_displayTypes->setObjectName(QString::fromUtf8("groupBox_displayTypes"));
        horizontalLayout_7 = new QHBoxLayout(groupBox_displayTypes);
        horizontalLayout_7->setObjectName(QString::fromUtf8("horizontalLayout_7"));
        kcfg_business = new QCheckBox(groupBox_displayTypes);
        kcfg_business->setObjectName(QString::fromUtf8("kcfg_business"));

        horizontalLayout_7->addWidget(kcfg_business);

        kcfg_mobile = new QCheckBox(groupBox_displayTypes);
        kcfg_mobile->setObjectName(QString::fromUtf8("kcfg_mobile"));

        horizontalLayout_7->addWidget(kcfg_mobile);

        kcfg_home = new QCheckBox(groupBox_displayTypes);
        kcfg_home->setObjectName(QString::fromUtf8("kcfg_home"));

        horizontalLayout_7->addWidget(kcfg_home);


        verticalLayout_2->addWidget(groupBox_displayTypes);


        verticalLayout->addWidget(widget_configAddressBookGeneral);

        verticalSpacer_configAddressBook = new QSpacerItem(20, 72, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_configAddressBook);

#ifndef UI_QT_NO_SHORTCUT
        label_maxResults->setBuddy(horizontalSlider_maxResults);
#endif // QT_NO_SHORTCUT

        retranslateUi(DlgAddressBookBase);
        QObject::connect(horizontalSlider_maxResults, SIGNAL(valueChanged(int)), kcfg_maxResults, SLOT(setValue(int)));
        QObject::connect(kcfg_maxResults, SIGNAL(valueChanged(int)), horizontalSlider_maxResults, SLOT(setValue(int)));
        QObject::connect(kcfg_enableAddressBook, SIGNAL(toggled(bool)), widget_configAddressBookGeneral, SLOT(setEnabled(bool)));

        QMetaObject::connectSlotsByName(DlgAddressBookBase);
    } // setupUi

    void retranslateUi(QWidget *DlgAddressBookBase)
    {
        kcfg_enableAddressBook->setText(tr2i18n("Enable address book", 0));
        label_maxResults->setText(tr2i18n("Maximum results", 0));
        kcfg_displayPhoto->setText(tr2i18n("Display photo if available", 0));
        groupBox_displayTypes->setTitle(tr2i18n("Display phone numbers of these types :", 0));
        kcfg_business->setText(tr2i18n("Work", 0));
        kcfg_mobile->setText(tr2i18n("Mobile", 0));
        kcfg_home->setText(tr2i18n("Home", 0));
        Q_UNUSED(DlgAddressBookBase);
    } // retranslateUi

};

namespace Ui {
    class DlgAddressBookBase: public Ui_DlgAddressBookBase {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DLGADDRESSBOOKBASE_H

