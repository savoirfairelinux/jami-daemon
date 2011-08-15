#include <kdialog.h>
#include <klocale.h>

/********************************************************************************
** Form generated from reading UI file 'dlgdisplaybase.ui'
**
** Created: Tue Apr 20 14:19:41 2010
**      by: Qt User Interface Compiler version 4.6.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DLGDISPLAYBASE_H
#define UI_DLGDISPLAYBASE_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QSpacerItem>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_DlgDisplayBase
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *label1_notifications;
    QWidget *widget1_notifications;
    QHBoxLayout *horizontalLayout_5;
    QCheckBox *kcfg_notifOnCalls;
    QCheckBox *kcfg_notifOnMessages;
    QLabel *label2_displayMainWindow;
    QWidget *widget_displayMainWindow;
    QHBoxLayout *horizontalLayout_6;
    QCheckBox *kcfg_displayOnStart;
    QCheckBox *kcfg_displayOnCalls;
    QSpacerItem *verticalSpacer_configDisplay;

    void setupUi(QWidget *DlgDisplayBase)
    {
        if (DlgDisplayBase->objectName().isEmpty())
            DlgDisplayBase->setObjectName(QString::fromUtf8("DlgDisplayBase"));
        DlgDisplayBase->resize(373, 300);
        DlgDisplayBase->setWindowTitle(QString::fromUtf8("Form"));
        verticalLayout = new QVBoxLayout(DlgDisplayBase);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        label1_notifications = new QLabel(DlgDisplayBase);
        label1_notifications->setObjectName(QString::fromUtf8("label1_notifications"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(label1_notifications->sizePolicy().hasHeightForWidth());
        label1_notifications->setSizePolicy(sizePolicy);

        verticalLayout->addWidget(label1_notifications);

        widget1_notifications = new QWidget(DlgDisplayBase);
        widget1_notifications->setObjectName(QString::fromUtf8("widget1_notifications"));
        sizePolicy.setHeightForWidth(widget1_notifications->sizePolicy().hasHeightForWidth());
        widget1_notifications->setSizePolicy(sizePolicy);
        horizontalLayout_5 = new QHBoxLayout(widget1_notifications);
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        kcfg_notifOnCalls = new QCheckBox(widget1_notifications);
        kcfg_notifOnCalls->setObjectName(QString::fromUtf8("kcfg_notifOnCalls"));

        horizontalLayout_5->addWidget(kcfg_notifOnCalls);

        kcfg_notifOnMessages = new QCheckBox(widget1_notifications);
        kcfg_notifOnMessages->setObjectName(QString::fromUtf8("kcfg_notifOnMessages"));

        horizontalLayout_5->addWidget(kcfg_notifOnMessages);


        verticalLayout->addWidget(widget1_notifications);

        label2_displayMainWindow = new QLabel(DlgDisplayBase);
        label2_displayMainWindow->setObjectName(QString::fromUtf8("label2_displayMainWindow"));
        sizePolicy.setHeightForWidth(label2_displayMainWindow->sizePolicy().hasHeightForWidth());
        label2_displayMainWindow->setSizePolicy(sizePolicy);

        verticalLayout->addWidget(label2_displayMainWindow);

        widget_displayMainWindow = new QWidget(DlgDisplayBase);
        widget_displayMainWindow->setObjectName(QString::fromUtf8("widget_displayMainWindow"));
        sizePolicy.setHeightForWidth(widget_displayMainWindow->sizePolicy().hasHeightForWidth());
        widget_displayMainWindow->setSizePolicy(sizePolicy);
        horizontalLayout_6 = new QHBoxLayout(widget_displayMainWindow);
        horizontalLayout_6->setObjectName(QString::fromUtf8("horizontalLayout_6"));
        kcfg_displayOnStart = new QCheckBox(widget_displayMainWindow);
        kcfg_displayOnStart->setObjectName(QString::fromUtf8("kcfg_displayOnStart"));

        horizontalLayout_6->addWidget(kcfg_displayOnStart);

        kcfg_displayOnCalls = new QCheckBox(widget_displayMainWindow);
        kcfg_displayOnCalls->setObjectName(QString::fromUtf8("kcfg_displayOnCalls"));

        horizontalLayout_6->addWidget(kcfg_displayOnCalls);


        verticalLayout->addWidget(widget_displayMainWindow);

        verticalSpacer_configDisplay = new QSpacerItem(20, 16777215, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_configDisplay);


        retranslateUi(DlgDisplayBase);

        QMetaObject::connectSlotsByName(DlgDisplayBase);
    } // setupUi

    void retranslateUi(QWidget *DlgDisplayBase)
    {
        label1_notifications->setText(tr2i18n("Enable notifications", 0));
        kcfg_notifOnCalls->setText(tr2i18n("On incoming calls", 0));
        kcfg_notifOnMessages->setText(tr2i18n("On messages", 0));
        label2_displayMainWindow->setText(tr2i18n("Show main window", 0));
        kcfg_displayOnStart->setText(tr2i18n("On start", 0));
        kcfg_displayOnCalls->setText(tr2i18n("On incoming calls", 0));
        Q_UNUSED(DlgDisplayBase);
    } // retranslateUi

};

namespace Ui {
    class DlgDisplayBase: public Ui_DlgDisplayBase {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DLGDISPLAYBASE_H

