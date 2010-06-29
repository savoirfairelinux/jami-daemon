#include <kdialog.h>
#include <klocale.h>

/********************************************************************************
** Form generated from reading UI file 'dlggeneralbase.ui'
**
** Created: Tue Apr 20 14:19:41 2010
**      by: Qt User Interface Compiler version 4.6.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DLGGENERALBASE_H
#define UI_DLGGENERALBASE_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QFormLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QSpacerItem>
#include <QtGui/QToolButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>
#include "knuminput.h"

QT_BEGIN_NAMESPACE

class Ui_DlgGeneralBase
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox1_history_2;
    QVBoxLayout *verticalLayout_21;
    QWidget *widget_historyCapacity_2;
    QHBoxLayout *horizontalLayout_11;
    QCheckBox *kcfg_enableHistory;
    KIntSpinBox *kcfg_historyMax;
    QLabel *label;
    QSpacerItem *horizontalSpacer;
    QToolButton *toolButton_historyClear;
    QGroupBox *groupBox2_connection_2;
    QFormLayout *formLayout_13;
    QLabel *label_SIPPort_2;
    QWidget *widget_SIPPort_2;
    QHBoxLayout *horizontalLayout_9;
    KIntSpinBox *kcfg_SIPPort;
    QLabel *label_WarningSIPPort;
    QSpacerItem *verticalSpacer_configGeneral_2;

    void setupUi(QWidget *DlgGeneralBase)
    {
        if (DlgGeneralBase->objectName().isEmpty())
            DlgGeneralBase->setObjectName(QString::fromUtf8("DlgGeneralBase"));
        DlgGeneralBase->resize(525, 404);
        DlgGeneralBase->setWindowTitle(QString::fromUtf8("Form"));
        verticalLayout = new QVBoxLayout(DlgGeneralBase);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        groupBox1_history_2 = new QGroupBox(DlgGeneralBase);
        groupBox1_history_2->setObjectName(QString::fromUtf8("groupBox1_history_2"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(groupBox1_history_2->sizePolicy().hasHeightForWidth());
        groupBox1_history_2->setSizePolicy(sizePolicy);
        verticalLayout_21 = new QVBoxLayout(groupBox1_history_2);
        verticalLayout_21->setObjectName(QString::fromUtf8("verticalLayout_21"));
        widget_historyCapacity_2 = new QWidget(groupBox1_history_2);
        widget_historyCapacity_2->setObjectName(QString::fromUtf8("widget_historyCapacity_2"));
        horizontalLayout_11 = new QHBoxLayout(widget_historyCapacity_2);
        horizontalLayout_11->setObjectName(QString::fromUtf8("horizontalLayout_11"));
        kcfg_enableHistory = new QCheckBox(widget_historyCapacity_2);
        kcfg_enableHistory->setObjectName(QString::fromUtf8("kcfg_enableHistory"));

        horizontalLayout_11->addWidget(kcfg_enableHistory);

        kcfg_historyMax = new KIntSpinBox(widget_historyCapacity_2);
        kcfg_historyMax->setObjectName(QString::fromUtf8("kcfg_historyMax"));
        sizePolicy.setHeightForWidth(kcfg_historyMax->sizePolicy().hasHeightForWidth());
        kcfg_historyMax->setSizePolicy(sizePolicy);
        kcfg_historyMax->setMinimum(-8);

        horizontalLayout_11->addWidget(kcfg_historyMax);

        label = new QLabel(widget_historyCapacity_2);
        label->setObjectName(QString::fromUtf8("label"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy1);

        horizontalLayout_11->addWidget(label);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_11->addItem(horizontalSpacer);


        verticalLayout_21->addWidget(widget_historyCapacity_2);

        toolButton_historyClear = new QToolButton(groupBox1_history_2);
        toolButton_historyClear->setObjectName(QString::fromUtf8("toolButton_historyClear"));

        verticalLayout_21->addWidget(toolButton_historyClear);


        verticalLayout->addWidget(groupBox1_history_2);

        groupBox2_connection_2 = new QGroupBox(DlgGeneralBase);
        groupBox2_connection_2->setObjectName(QString::fromUtf8("groupBox2_connection_2"));
        sizePolicy.setHeightForWidth(groupBox2_connection_2->sizePolicy().hasHeightForWidth());
        groupBox2_connection_2->setSizePolicy(sizePolicy);
        formLayout_13 = new QFormLayout(groupBox2_connection_2);
        formLayout_13->setObjectName(QString::fromUtf8("formLayout_13"));
        formLayout_13->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        label_SIPPort_2 = new QLabel(groupBox2_connection_2);
        label_SIPPort_2->setObjectName(QString::fromUtf8("label_SIPPort_2"));

        formLayout_13->setWidget(0, QFormLayout::LabelRole, label_SIPPort_2);

        widget_SIPPort_2 = new QWidget(groupBox2_connection_2);
        widget_SIPPort_2->setObjectName(QString::fromUtf8("widget_SIPPort_2"));
        widget_SIPPort_2->setMinimumSize(QSize(50, 0));
        horizontalLayout_9 = new QHBoxLayout(widget_SIPPort_2);
        horizontalLayout_9->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_9->setObjectName(QString::fromUtf8("horizontalLayout_9"));
        kcfg_SIPPort = new KIntSpinBox(widget_SIPPort_2);
        kcfg_SIPPort->setObjectName(QString::fromUtf8("kcfg_SIPPort"));

        horizontalLayout_9->addWidget(kcfg_SIPPort);

        label_WarningSIPPort = new QLabel(widget_SIPPort_2);
        label_WarningSIPPort->setObjectName(QString::fromUtf8("label_WarningSIPPort"));
        label_WarningSIPPort->setEnabled(false);

        horizontalLayout_9->addWidget(label_WarningSIPPort);


        formLayout_13->setWidget(0, QFormLayout::FieldRole, widget_SIPPort_2);


        verticalLayout->addWidget(groupBox2_connection_2);

        verticalSpacer_configGeneral_2 = new QSpacerItem(504, 171, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_configGeneral_2);

#ifndef UI_QT_NO_SHORTCUT
        label_SIPPort_2->setBuddy(kcfg_SIPPort);
#endif // QT_NO_SHORTCUT

        retranslateUi(DlgGeneralBase);

        QMetaObject::connectSlotsByName(DlgGeneralBase);
    } // setupUi

    void retranslateUi(QWidget *DlgGeneralBase)
    {
        groupBox1_history_2->setTitle(tr2i18n("Call history", 0));
        kcfg_enableHistory->setText(tr2i18n("Keep my history for at least", 0));
        label->setText(tr2i18n("days", 0));
        toolButton_historyClear->setText(tr2i18n("Clear history", 0));
        groupBox2_connection_2->setTitle(tr2i18n("Connection", 0));
        label_SIPPort_2->setText(tr2i18n("SIP Port", 0));
        label_WarningSIPPort->setText(QString());
        Q_UNUSED(DlgGeneralBase);
    } // retranslateUi

};

namespace Ui {
    class DlgGeneralBase: public Ui_DlgGeneralBase {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DLGGENERALBASE_H

