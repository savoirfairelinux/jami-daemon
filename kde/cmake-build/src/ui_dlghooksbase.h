#include <kdialog.h>
#include <klocale.h>

/********************************************************************************
** Form generated from reading UI file 'dlghooksbase.ui'
**
** Created: Tue Apr 20 14:19:42 2010
**      by: Qt User Interface Compiler version 4.6.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DLGHOOKSBASE_H
#define UI_DLGHOOKSBASE_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QGroupBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QSpacerItem>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>
#include "klineedit.h"

QT_BEGIN_NAMESPACE

class Ui_DlgHooksBase
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox_urlArgument;
    QVBoxLayout *verticalLayout_12;
    QLabel *label;
    QWidget *widget_protocols;
    QHBoxLayout *horizontalLayout_8;
    QCheckBox *kcfg_enableHooksSIP;
    QLineEdit *kcfg_hooksSIPHeader;
    QCheckBox *kcfg_enableHooksIAX;
    QWidget *widget_urlArgumentForm;
    QHBoxLayout *horizontalLayout;
    QLabel *label_command;
    KLineEdit *kcfg_hooksCommand;
    QLabel *label_2;
    QGroupBox *groupBox_phoneNumberFormatting;
    QVBoxLayout *verticalLayout_13;
    QWidget *widget_phoneNumberFormattingForm;
    QHBoxLayout *horizontalLayout_2;
    QCheckBox *kcfg_addPrefix;
    KLineEdit *kcfg_prepend;
    QSpacerItem *verticalSpacer_configHooks;

    void setupUi(QWidget *DlgHooksBase)
    {
        if (DlgHooksBase->objectName().isEmpty())
            DlgHooksBase->setObjectName(QString::fromUtf8("DlgHooksBase"));
        DlgHooksBase->resize(520, 407);
        DlgHooksBase->setWindowTitle(QString::fromUtf8("Form"));
        verticalLayout = new QVBoxLayout(DlgHooksBase);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        groupBox_urlArgument = new QGroupBox(DlgHooksBase);
        groupBox_urlArgument->setObjectName(QString::fromUtf8("groupBox_urlArgument"));
        verticalLayout_12 = new QVBoxLayout(groupBox_urlArgument);
        verticalLayout_12->setObjectName(QString::fromUtf8("verticalLayout_12"));
        label = new QLabel(groupBox_urlArgument);
        label->setObjectName(QString::fromUtf8("label"));
        label->setWordWrap(true);

        verticalLayout_12->addWidget(label);

        widget_protocols = new QWidget(groupBox_urlArgument);
        widget_protocols->setObjectName(QString::fromUtf8("widget_protocols"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(widget_protocols->sizePolicy().hasHeightForWidth());
        widget_protocols->setSizePolicy(sizePolicy);
        widget_protocols->setMinimumSize(QSize(0, 0));
        horizontalLayout_8 = new QHBoxLayout(widget_protocols);
        horizontalLayout_8->setObjectName(QString::fromUtf8("horizontalLayout_8"));
        horizontalLayout_8->setContentsMargins(0, 4, 4, 4);
        kcfg_enableHooksSIP = new QCheckBox(widget_protocols);
        kcfg_enableHooksSIP->setObjectName(QString::fromUtf8("kcfg_enableHooksSIP"));

        horizontalLayout_8->addWidget(kcfg_enableHooksSIP);

        kcfg_hooksSIPHeader = new QLineEdit(widget_protocols);
        kcfg_hooksSIPHeader->setObjectName(QString::fromUtf8("kcfg_hooksSIPHeader"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(kcfg_hooksSIPHeader->sizePolicy().hasHeightForWidth());
        kcfg_hooksSIPHeader->setSizePolicy(sizePolicy1);

        horizontalLayout_8->addWidget(kcfg_hooksSIPHeader);


        verticalLayout_12->addWidget(widget_protocols);

        kcfg_enableHooksIAX = new QCheckBox(groupBox_urlArgument);
        kcfg_enableHooksIAX->setObjectName(QString::fromUtf8("kcfg_enableHooksIAX"));

        verticalLayout_12->addWidget(kcfg_enableHooksIAX);

        widget_urlArgumentForm = new QWidget(groupBox_urlArgument);
        widget_urlArgumentForm->setObjectName(QString::fromUtf8("widget_urlArgumentForm"));
        horizontalLayout = new QHBoxLayout(widget_urlArgumentForm);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 4, 4, 4);
        label_command = new QLabel(widget_urlArgumentForm);
        label_command->setObjectName(QString::fromUtf8("label_command"));

        horizontalLayout->addWidget(label_command);

        kcfg_hooksCommand = new KLineEdit(widget_urlArgumentForm);
        kcfg_hooksCommand->setObjectName(QString::fromUtf8("kcfg_hooksCommand"));
        sizePolicy1.setHeightForWidth(kcfg_hooksCommand->sizePolicy().hasHeightForWidth());
        kcfg_hooksCommand->setSizePolicy(sizePolicy1);

        horizontalLayout->addWidget(kcfg_hooksCommand);


        verticalLayout_12->addWidget(widget_urlArgumentForm);

        label_2 = new QLabel(groupBox_urlArgument);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        verticalLayout_12->addWidget(label_2);


        verticalLayout->addWidget(groupBox_urlArgument);

        groupBox_phoneNumberFormatting = new QGroupBox(DlgHooksBase);
        groupBox_phoneNumberFormatting->setObjectName(QString::fromUtf8("groupBox_phoneNumberFormatting"));
        verticalLayout_13 = new QVBoxLayout(groupBox_phoneNumberFormatting);
        verticalLayout_13->setObjectName(QString::fromUtf8("verticalLayout_13"));
        widget_phoneNumberFormattingForm = new QWidget(groupBox_phoneNumberFormatting);
        widget_phoneNumberFormattingForm->setObjectName(QString::fromUtf8("widget_phoneNumberFormattingForm"));
        horizontalLayout_2 = new QHBoxLayout(widget_phoneNumberFormattingForm);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalLayout_2->setContentsMargins(0, 4, 4, 4);
        kcfg_addPrefix = new QCheckBox(widget_phoneNumberFormattingForm);
        kcfg_addPrefix->setObjectName(QString::fromUtf8("kcfg_addPrefix"));

        horizontalLayout_2->addWidget(kcfg_addPrefix);

        kcfg_prepend = new KLineEdit(widget_phoneNumberFormattingForm);
        kcfg_prepend->setObjectName(QString::fromUtf8("kcfg_prepend"));
        sizePolicy1.setHeightForWidth(kcfg_prepend->sizePolicy().hasHeightForWidth());
        kcfg_prepend->setSizePolicy(sizePolicy1);

        horizontalLayout_2->addWidget(kcfg_prepend);


        verticalLayout_13->addWidget(widget_phoneNumberFormattingForm);


        verticalLayout->addWidget(groupBox_phoneNumberFormatting);

        verticalSpacer_configHooks = new QSpacerItem(499, 96, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_configHooks);

#ifndef UI_QT_NO_SHORTCUT
        label_command->setBuddy(kcfg_hooksCommand);
#endif // QT_NO_SHORTCUT

        retranslateUi(DlgHooksBase);

        QMetaObject::connectSlotsByName(DlgHooksBase);
    } // setupUi

    void retranslateUi(QWidget *DlgHooksBase)
    {
        groupBox_urlArgument->setTitle(tr2i18n("URL Argument", 0));
        label->setText(tr2i18n("Custom commands on incoming calls with URL", 0));
        kcfg_enableHooksSIP->setText(tr2i18n("Trigger on specific SIP header", 0));
        kcfg_enableHooksIAX->setText(tr2i18n("Trigger on IAX2 URL", 0));
        label_command->setText(tr2i18n("Command to run", 0));
        label_2->setText(tr2i18n("%s will be replaced with the passed URL.", 0));
        groupBox_phoneNumberFormatting->setTitle(tr2i18n("Phone number rewriting", 0));
        kcfg_addPrefix->setText(tr2i18n("Prefix dialed numbers with", 0));
        Q_UNUSED(DlgHooksBase);
    } // retranslateUi

};

namespace Ui {
    class DlgHooksBase: public Ui_DlgHooksBase {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DLGHOOKSBASE_H

