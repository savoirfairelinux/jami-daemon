#include <kdialog.h>
#include <klocale.h>

/********************************************************************************
** Form generated from reading UI file 'dlgaccountsbase.ui'
**
** Created: Tue Apr 20 14:19:41 2010
**      by: Qt User Interface Compiler version 4.6.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DLGACCOUNTSBASE_H
#define UI_DLGACCOUNTSBASE_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QFormLayout>
#include <QtGui/QFrame>
#include <QtGui/QGridLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QListWidget>
#include <QtGui/QRadioButton>
#include <QtGui/QScrollArea>
#include <QtGui/QSpacerItem>
#include <QtGui/QSpinBox>
#include <QtGui/QTabWidget>
#include <QtGui/QToolButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>
#include "keditlistbox.h"
#include "klineedit.h"
#include "knuminput.h"
#include "kurlrequester.h"

QT_BEGIN_NAMESPACE

class Ui_DlgAccountsBase
{
public:
    QVBoxLayout *verticalLayout;
    QWidget *widget1_configAccounts;
    QHBoxLayout *horizontalLayout_3;
    QFrame *frame1_accountList;
    QVBoxLayout *verticalLayout_6;
    QListWidget *listWidget_accountList;
    QGroupBox *groupBox_accountListHandle;
    QHBoxLayout *horizontalLayout_2;
    QToolButton *button_accountRemove;
    QToolButton *button_accountAdd;
    QToolButton *button_accountDown;
    QToolButton *button_accountUp;
    QSpacerItem *horizontalSpacer;
    QTabWidget *frame2_editAccounts;
    QWidget *tab_basic;
    QFormLayout *formLayout;
    QLabel *label1_alias;
    QLineEdit *edit1_alias;
    QLabel *label2_protocol;
    QComboBox *edit2_protocol;
    QLabel *label3_server;
    QLineEdit *edit3_server;
    QLabel *label4_user;
    QLineEdit *edit4_user;
    QLabel *label5_password;
    QLineEdit *edit5_password;
    QLabel *label6_mailbox;
    QLineEdit *edit6_mailbox;
    QLabel *label7_state;
    QLabel *edit7_state;
    QWidget *tab_advanced;
    QVBoxLayout *verticalLayout_3;
    QGroupBox *groupBox;
    QGridLayout *gridLayout_8;
    QLabel *label_regExpire;
    KIntSpinBox *spinbox_regExpire;
    QCheckBox *checkBox_conformRFC;
    QSpacerItem *horizontalSpacer_7;
    QGroupBox *groupBox_2;
    QGridLayout *gridLayout_9;
    QLabel *label_ni_local_address;
    QComboBox *comboBox_ni_local_address;
    QLabel *label_ni_local_port;
    QSpinBox *spinBox_ni_local_port;
    QSpacerItem *horizontalSpacer_8;
    QGroupBox *groupBox_3;
    QGridLayout *gridLayout_10;
    QRadioButton *radioButton_pa_same_as_local;
    QRadioButton *radioButton_pa_custom;
    QLabel *label_published_address;
    QLabel *label_pa_published_port;
    QSpinBox *spinBox_pa_published_port;
    QLineEdit *lineEdit_pa_published_address;
    QSpacerItem *horizontalSpacer_9;
    QSpacerItem *verticalSpacer_3;
    QWidget *tab_stun;
    QGridLayout *gridLayout_6;
    QLabel *label_commonSettings;
    QSpacerItem *verticalSpacer_2;
    QCheckBox *checkbox_stun;
    KLineEdit *line_stun;
    QWidget *tab_codec;
    QGridLayout *gridLayout_7;
    KEditListBox *keditlistbox_codec;
    QLabel *label_frequency;
    QLabel *label_frequency_value;
    QSpacerItem *horizontalSpacer_6;
    QLabel *label_bitrate;
    QLabel *label_bitrate_value;
    QLabel *label_bandwidth;
    QLabel *label_bandwidth_value;
    QWidget *tab;
    QGridLayout *gridLayout;
    QListWidget *list_credential;
    QSpacerItem *horizontalSpacer_2;
    QGroupBox *group_credential;
    QGridLayout *gridLayout_2;
    QLabel *label_credential_realm;
    QLabel *labe_credential_auth;
    QLabel *label_credential_password;
    KLineEdit *edit_credential_realm;
    KLineEdit *edit_credential_auth;
    KLineEdit *edit_credential_password;
    QToolButton *button_add_credential;
    QToolButton *button_remove_credential;
    QWidget *tab_2;
    QGridLayout *gridLayout_3;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QGridLayout *gridLayout_5;
    QLabel *label_tls_info;
    QGroupBox *group_security_tls;
    QGridLayout *gridLayout_4;
    QLabel *label_tls_listener;
    KIntSpinBox *spinbox_tls_listener;
    QSpacerItem *horizontalSpacer_4;
    QLabel *label_tls_authority;
    KUrlRequester *file_tls_authority;
    QLabel *label_tls_endpoint;
    KUrlRequester *file_tls_endpoint;
    QLabel *label_tls_private_key;
    KUrlRequester *file_tls_private_key;
    QLabel *label_tls_private_key_password;
    KLineEdit *edit_tls_private_key_password;
    QLabel *label_tls_method;
    QComboBox *combo_tls_method;
    QSpacerItem *horizontalSpacer_5;
    QLabel *label_tls_cipher;
    KLineEdit *edit_tls_cipher;
    QLabel *label_tls_outgoing;
    KLineEdit *edit_tls_outgoing;
    QLabel *label_tls_timeout;
    KIntSpinBox *spinbox_tls_timeout_sec;
    QCheckBox *check_tls_incoming;
    QCheckBox *check_tls_answer;
    QCheckBox *check_tls_requier_cert;
    QLabel *label_tls_details;
    QSpacerItem *verticalSpacer;
    QSpacerItem *horizontalSpacer_3;
    KIntSpinBox *spinbox_tls_timeout_msec;
    QLabel *label_timeout2;
    QGroupBox *groupbox_STRP_keyexchange;
    QVBoxLayout *verticalLayout_2;
    QComboBox *combo_security_STRP;
    QCheckBox *checkbox_ZTRP_send_hello;
    QCheckBox *checkbox_ZRTP_warn_supported;
    QCheckBox *checkbox_ZRTP_Ask_user;
    QCheckBox *checkbox_ZRTP_display_SAS;
    QCheckBox *checkbox_SDES_fallback_rtp;

    void setupUi(QWidget *DlgAccountsBase)
    {
        if (DlgAccountsBase->objectName().isEmpty())
            DlgAccountsBase->setObjectName(QString::fromUtf8("DlgAccountsBase"));
        DlgAccountsBase->resize(858, 550);
        DlgAccountsBase->setWindowTitle(QString::fromUtf8("Form"));
        verticalLayout = new QVBoxLayout(DlgAccountsBase);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        widget1_configAccounts = new QWidget(DlgAccountsBase);
        widget1_configAccounts->setObjectName(QString::fromUtf8("widget1_configAccounts"));
        widget1_configAccounts->setAutoFillBackground(false);
        horizontalLayout_3 = new QHBoxLayout(widget1_configAccounts);
        horizontalLayout_3->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalLayout_3->setSizeConstraint(QLayout::SetDefaultConstraint);
        frame1_accountList = new QFrame(widget1_configAccounts);
        frame1_accountList->setObjectName(QString::fromUtf8("frame1_accountList"));
        QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(frame1_accountList->sizePolicy().hasHeightForWidth());
        frame1_accountList->setSizePolicy(sizePolicy);
        frame1_accountList->setMinimumSize(QSize(0, 0));
        frame1_accountList->setMaximumSize(QSize(16777215, 16777215));
        frame1_accountList->setSizeIncrement(QSize(0, 0));
        frame1_accountList->setFrameShape(QFrame::StyledPanel);
        frame1_accountList->setFrameShadow(QFrame::Raised);
        verticalLayout_6 = new QVBoxLayout(frame1_accountList);
        verticalLayout_6->setObjectName(QString::fromUtf8("verticalLayout_6"));
        listWidget_accountList = new QListWidget(frame1_accountList);
        listWidget_accountList->setObjectName(QString::fromUtf8("listWidget_accountList"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(listWidget_accountList->sizePolicy().hasHeightForWidth());
        listWidget_accountList->setSizePolicy(sizePolicy1);
        listWidget_accountList->setMinimumSize(QSize(150, 0));
        listWidget_accountList->setMaximumSize(QSize(16777215, 16777215));
        listWidget_accountList->setDragEnabled(true);

        verticalLayout_6->addWidget(listWidget_accountList);

        groupBox_accountListHandle = new QGroupBox(frame1_accountList);
        groupBox_accountListHandle->setObjectName(QString::fromUtf8("groupBox_accountListHandle"));
        QSizePolicy sizePolicy2(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(groupBox_accountListHandle->sizePolicy().hasHeightForWidth());
        groupBox_accountListHandle->setSizePolicy(sizePolicy2);
        groupBox_accountListHandle->setMaximumSize(QSize(16777215, 16777215));
        groupBox_accountListHandle->setLayoutDirection(Qt::RightToLeft);
        groupBox_accountListHandle->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        horizontalLayout_2 = new QHBoxLayout(groupBox_accountListHandle);
        horizontalLayout_2->setSpacing(0);
        horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalLayout_2->setSizeConstraint(QLayout::SetNoConstraint);
        button_accountRemove = new QToolButton(groupBox_accountListHandle);
        button_accountRemove->setObjectName(QString::fromUtf8("button_accountRemove"));
        QSizePolicy sizePolicy3(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(button_accountRemove->sizePolicy().hasHeightForWidth());
        button_accountRemove->setSizePolicy(sizePolicy3);
        button_accountRemove->setShortcut(QString::fromUtf8("-, Del, Backspace"));

        horizontalLayout_2->addWidget(button_accountRemove);

        button_accountAdd = new QToolButton(groupBox_accountListHandle);
        button_accountAdd->setObjectName(QString::fromUtf8("button_accountAdd"));
        sizePolicy3.setHeightForWidth(button_accountAdd->sizePolicy().hasHeightForWidth());
        button_accountAdd->setSizePolicy(sizePolicy3);
        button_accountAdd->setSizeIncrement(QSize(0, 0));
        button_accountAdd->setShortcut(QString::fromUtf8("+"));

        horizontalLayout_2->addWidget(button_accountAdd);

        button_accountDown = new QToolButton(groupBox_accountListHandle);
        button_accountDown->setObjectName(QString::fromUtf8("button_accountDown"));
        button_accountDown->setShortcut(QString::fromUtf8("Down, PgDown"));

        horizontalLayout_2->addWidget(button_accountDown);

        button_accountUp = new QToolButton(groupBox_accountListHandle);
        button_accountUp->setObjectName(QString::fromUtf8("button_accountUp"));
        button_accountUp->setShortcut(QString::fromUtf8("Up, PgUp"));

        horizontalLayout_2->addWidget(button_accountUp);

        horizontalSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);


        verticalLayout_6->addWidget(groupBox_accountListHandle);


        horizontalLayout_3->addWidget(frame1_accountList);

        frame2_editAccounts = new QTabWidget(widget1_configAccounts);
        frame2_editAccounts->setObjectName(QString::fromUtf8("frame2_editAccounts"));
        QSizePolicy sizePolicy4(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy4.setHorizontalStretch(3);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(frame2_editAccounts->sizePolicy().hasHeightForWidth());
        frame2_editAccounts->setSizePolicy(sizePolicy4);
        tab_basic = new QWidget();
        tab_basic->setObjectName(QString::fromUtf8("tab_basic"));
        formLayout = new QFormLayout(tab_basic);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        label1_alias = new QLabel(tab_basic);
        label1_alias->setObjectName(QString::fromUtf8("label1_alias"));

        formLayout->setWidget(0, QFormLayout::LabelRole, label1_alias);

        edit1_alias = new QLineEdit(tab_basic);
        edit1_alias->setObjectName(QString::fromUtf8("edit1_alias"));
        QSizePolicy sizePolicy5(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy5.setHorizontalStretch(0);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(edit1_alias->sizePolicy().hasHeightForWidth());
        edit1_alias->setSizePolicy(sizePolicy5);
        edit1_alias->setMinimumSize(QSize(0, 0));

        formLayout->setWidget(0, QFormLayout::FieldRole, edit1_alias);

        label2_protocol = new QLabel(tab_basic);
        label2_protocol->setObjectName(QString::fromUtf8("label2_protocol"));

        formLayout->setWidget(1, QFormLayout::LabelRole, label2_protocol);

        edit2_protocol = new QComboBox(tab_basic);
        edit2_protocol->setObjectName(QString::fromUtf8("edit2_protocol"));

        formLayout->setWidget(1, QFormLayout::FieldRole, edit2_protocol);

        label3_server = new QLabel(tab_basic);
        label3_server->setObjectName(QString::fromUtf8("label3_server"));

        formLayout->setWidget(2, QFormLayout::LabelRole, label3_server);

        edit3_server = new QLineEdit(tab_basic);
        edit3_server->setObjectName(QString::fromUtf8("edit3_server"));
        edit3_server->setMinimumSize(QSize(0, 0));

        formLayout->setWidget(2, QFormLayout::FieldRole, edit3_server);

        label4_user = new QLabel(tab_basic);
        label4_user->setObjectName(QString::fromUtf8("label4_user"));

        formLayout->setWidget(3, QFormLayout::LabelRole, label4_user);

        edit4_user = new QLineEdit(tab_basic);
        edit4_user->setObjectName(QString::fromUtf8("edit4_user"));

        formLayout->setWidget(3, QFormLayout::FieldRole, edit4_user);

        label5_password = new QLabel(tab_basic);
        label5_password->setObjectName(QString::fromUtf8("label5_password"));

        formLayout->setWidget(4, QFormLayout::LabelRole, label5_password);

        edit5_password = new QLineEdit(tab_basic);
        edit5_password->setObjectName(QString::fromUtf8("edit5_password"));
        edit5_password->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(4, QFormLayout::FieldRole, edit5_password);

        label6_mailbox = new QLabel(tab_basic);
        label6_mailbox->setObjectName(QString::fromUtf8("label6_mailbox"));

        formLayout->setWidget(5, QFormLayout::LabelRole, label6_mailbox);

        edit6_mailbox = new QLineEdit(tab_basic);
        edit6_mailbox->setObjectName(QString::fromUtf8("edit6_mailbox"));

        formLayout->setWidget(5, QFormLayout::FieldRole, edit6_mailbox);

        label7_state = new QLabel(tab_basic);
        label7_state->setObjectName(QString::fromUtf8("label7_state"));

        formLayout->setWidget(6, QFormLayout::LabelRole, label7_state);

        edit7_state = new QLabel(tab_basic);
        edit7_state->setObjectName(QString::fromUtf8("edit7_state"));
        QSizePolicy sizePolicy6(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy6.setHorizontalStretch(0);
        sizePolicy6.setVerticalStretch(0);
        sizePolicy6.setHeightForWidth(edit7_state->sizePolicy().hasHeightForWidth());
        edit7_state->setSizePolicy(sizePolicy6);
        edit7_state->setMinimumSize(QSize(10, 0));

        formLayout->setWidget(6, QFormLayout::FieldRole, edit7_state);

        frame2_editAccounts->addTab(tab_basic, QString());
        tab_advanced = new QWidget();
        tab_advanced->setObjectName(QString::fromUtf8("tab_advanced"));
        verticalLayout_3 = new QVBoxLayout(tab_advanced);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        groupBox = new QGroupBox(tab_advanced);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        gridLayout_8 = new QGridLayout(groupBox);
        gridLayout_8->setObjectName(QString::fromUtf8("gridLayout_8"));
        label_regExpire = new QLabel(groupBox);
        label_regExpire->setObjectName(QString::fromUtf8("label_regExpire"));

        gridLayout_8->addWidget(label_regExpire, 0, 0, 1, 1);

        spinbox_regExpire = new KIntSpinBox(groupBox);
        spinbox_regExpire->setObjectName(QString::fromUtf8("spinbox_regExpire"));
        spinbox_regExpire->setMaximum(16777215);

        gridLayout_8->addWidget(spinbox_regExpire, 0, 1, 1, 1);

        checkBox_conformRFC = new QCheckBox(groupBox);
        checkBox_conformRFC->setObjectName(QString::fromUtf8("checkBox_conformRFC"));

        gridLayout_8->addWidget(checkBox_conformRFC, 1, 0, 1, 2);

        horizontalSpacer_7 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_8->addItem(horizontalSpacer_7, 0, 2, 1, 1);


        verticalLayout_3->addWidget(groupBox);

        groupBox_2 = new QGroupBox(tab_advanced);
        groupBox_2->setObjectName(QString::fromUtf8("groupBox_2"));
        gridLayout_9 = new QGridLayout(groupBox_2);
        gridLayout_9->setObjectName(QString::fromUtf8("gridLayout_9"));
        label_ni_local_address = new QLabel(groupBox_2);
        label_ni_local_address->setObjectName(QString::fromUtf8("label_ni_local_address"));

        gridLayout_9->addWidget(label_ni_local_address, 0, 0, 1, 1);

        comboBox_ni_local_address = new QComboBox(groupBox_2);
        comboBox_ni_local_address->setObjectName(QString::fromUtf8("comboBox_ni_local_address"));

        gridLayout_9->addWidget(comboBox_ni_local_address, 0, 1, 1, 1);

        label_ni_local_port = new QLabel(groupBox_2);
        label_ni_local_port->setObjectName(QString::fromUtf8("label_ni_local_port"));

        gridLayout_9->addWidget(label_ni_local_port, 1, 0, 1, 1);

        spinBox_ni_local_port = new QSpinBox(groupBox_2);
        spinBox_ni_local_port->setObjectName(QString::fromUtf8("spinBox_ni_local_port"));

        gridLayout_9->addWidget(spinBox_ni_local_port, 1, 1, 1, 1);

        horizontalSpacer_8 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_9->addItem(horizontalSpacer_8, 0, 2, 1, 1);


        verticalLayout_3->addWidget(groupBox_2);

        groupBox_3 = new QGroupBox(tab_advanced);
        groupBox_3->setObjectName(QString::fromUtf8("groupBox_3"));
        gridLayout_10 = new QGridLayout(groupBox_3);
        gridLayout_10->setObjectName(QString::fromUtf8("gridLayout_10"));
        radioButton_pa_same_as_local = new QRadioButton(groupBox_3);
        radioButton_pa_same_as_local->setObjectName(QString::fromUtf8("radioButton_pa_same_as_local"));

        gridLayout_10->addWidget(radioButton_pa_same_as_local, 0, 0, 1, 3);

        radioButton_pa_custom = new QRadioButton(groupBox_3);
        radioButton_pa_custom->setObjectName(QString::fromUtf8("radioButton_pa_custom"));

        gridLayout_10->addWidget(radioButton_pa_custom, 1, 0, 2, 3);

        label_published_address = new QLabel(groupBox_3);
        label_published_address->setObjectName(QString::fromUtf8("label_published_address"));

        gridLayout_10->addWidget(label_published_address, 3, 0, 1, 1);

        label_pa_published_port = new QLabel(groupBox_3);
        label_pa_published_port->setObjectName(QString::fromUtf8("label_pa_published_port"));

        gridLayout_10->addWidget(label_pa_published_port, 4, 0, 1, 1);

        spinBox_pa_published_port = new QSpinBox(groupBox_3);
        spinBox_pa_published_port->setObjectName(QString::fromUtf8("spinBox_pa_published_port"));

        gridLayout_10->addWidget(spinBox_pa_published_port, 4, 1, 1, 2);

        lineEdit_pa_published_address = new QLineEdit(groupBox_3);
        lineEdit_pa_published_address->setObjectName(QString::fromUtf8("lineEdit_pa_published_address"));

        gridLayout_10->addWidget(lineEdit_pa_published_address, 3, 1, 1, 1);

        horizontalSpacer_9 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_10->addItem(horizontalSpacer_9, 3, 3, 1, 1);


        verticalLayout_3->addWidget(groupBox_3);

        verticalSpacer_3 = new QSpacerItem(20, 138, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_3->addItem(verticalSpacer_3);

        frame2_editAccounts->addTab(tab_advanced, QString());
        tab_stun = new QWidget();
        tab_stun->setObjectName(QString::fromUtf8("tab_stun"));
        gridLayout_6 = new QGridLayout(tab_stun);
        gridLayout_6->setObjectName(QString::fromUtf8("gridLayout_6"));
        label_commonSettings = new QLabel(tab_stun);
        label_commonSettings->setObjectName(QString::fromUtf8("label_commonSettings"));
        label_commonSettings->setWordWrap(true);

        gridLayout_6->addWidget(label_commonSettings, 0, 0, 1, 2);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_6->addItem(verticalSpacer_2, 2, 0, 1, 1);

        checkbox_stun = new QCheckBox(tab_stun);
        checkbox_stun->setObjectName(QString::fromUtf8("checkbox_stun"));

        gridLayout_6->addWidget(checkbox_stun, 1, 0, 1, 1);

        line_stun = new KLineEdit(tab_stun);
        line_stun->setObjectName(QString::fromUtf8("line_stun"));
        line_stun->setEnabled(false);

        gridLayout_6->addWidget(line_stun, 1, 1, 1, 1);

        frame2_editAccounts->addTab(tab_stun, QString());
        tab_codec = new QWidget();
        tab_codec->setObjectName(QString::fromUtf8("tab_codec"));
        gridLayout_7 = new QGridLayout(tab_codec);
        gridLayout_7->setObjectName(QString::fromUtf8("gridLayout_7"));
        keditlistbox_codec = new KEditListBox(tab_codec);
        keditlistbox_codec->setObjectName(QString::fromUtf8("keditlistbox_codec"));
        keditlistbox_codec->setButtons(KEditListBox::All);

        gridLayout_7->addWidget(keditlistbox_codec, 0, 0, 1, 3);

        label_frequency = new QLabel(tab_codec);
        label_frequency->setObjectName(QString::fromUtf8("label_frequency"));

        gridLayout_7->addWidget(label_frequency, 1, 0, 1, 1);

        label_frequency_value = new QLabel(tab_codec);
        label_frequency_value->setObjectName(QString::fromUtf8("label_frequency_value"));

        gridLayout_7->addWidget(label_frequency_value, 1, 1, 1, 1);

        horizontalSpacer_6 = new QSpacerItem(470, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_7->addItem(horizontalSpacer_6, 1, 2, 2, 1);

        label_bitrate = new QLabel(tab_codec);
        label_bitrate->setObjectName(QString::fromUtf8("label_bitrate"));

        gridLayout_7->addWidget(label_bitrate, 2, 0, 1, 1);

        label_bitrate_value = new QLabel(tab_codec);
        label_bitrate_value->setObjectName(QString::fromUtf8("label_bitrate_value"));

        gridLayout_7->addWidget(label_bitrate_value, 2, 1, 1, 1);

        label_bandwidth = new QLabel(tab_codec);
        label_bandwidth->setObjectName(QString::fromUtf8("label_bandwidth"));

        gridLayout_7->addWidget(label_bandwidth, 3, 0, 1, 1);

        label_bandwidth_value = new QLabel(tab_codec);
        label_bandwidth_value->setObjectName(QString::fromUtf8("label_bandwidth_value"));

        gridLayout_7->addWidget(label_bandwidth_value, 3, 1, 1, 1);

        frame2_editAccounts->addTab(tab_codec, QString());
        tab = new QWidget();
        tab->setObjectName(QString::fromUtf8("tab"));
        gridLayout = new QGridLayout(tab);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        list_credential = new QListWidget(tab);
        list_credential->setObjectName(QString::fromUtf8("list_credential"));
        sizePolicy1.setHeightForWidth(list_credential->sizePolicy().hasHeightForWidth());
        list_credential->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(list_credential, 0, 0, 2, 3);

        horizontalSpacer_2 = new QSpacerItem(327, 23, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_2, 2, 0, 1, 1);

        group_credential = new QGroupBox(tab);
        group_credential->setObjectName(QString::fromUtf8("group_credential"));
        QSizePolicy sizePolicy7(QSizePolicy::Preferred, QSizePolicy::Minimum);
        sizePolicy7.setHorizontalStretch(0);
        sizePolicy7.setVerticalStretch(0);
        sizePolicy7.setHeightForWidth(group_credential->sizePolicy().hasHeightForWidth());
        group_credential->setSizePolicy(sizePolicy7);
        gridLayout_2 = new QGridLayout(group_credential);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        label_credential_realm = new QLabel(group_credential);
        label_credential_realm->setObjectName(QString::fromUtf8("label_credential_realm"));

        gridLayout_2->addWidget(label_credential_realm, 0, 0, 1, 1);

        labe_credential_auth = new QLabel(group_credential);
        labe_credential_auth->setObjectName(QString::fromUtf8("labe_credential_auth"));

        gridLayout_2->addWidget(labe_credential_auth, 1, 0, 1, 1);

        label_credential_password = new QLabel(group_credential);
        label_credential_password->setObjectName(QString::fromUtf8("label_credential_password"));

        gridLayout_2->addWidget(label_credential_password, 2, 0, 1, 1);

        edit_credential_realm = new KLineEdit(group_credential);
        edit_credential_realm->setObjectName(QString::fromUtf8("edit_credential_realm"));
        edit_credential_realm->setEnabled(false);

        gridLayout_2->addWidget(edit_credential_realm, 0, 1, 1, 1);

        edit_credential_auth = new KLineEdit(group_credential);
        edit_credential_auth->setObjectName(QString::fromUtf8("edit_credential_auth"));
        edit_credential_auth->setEnabled(false);

        gridLayout_2->addWidget(edit_credential_auth, 1, 1, 1, 1);

        edit_credential_password = new KLineEdit(group_credential);
        edit_credential_password->setObjectName(QString::fromUtf8("edit_credential_password"));
        edit_credential_password->setEnabled(false);

        gridLayout_2->addWidget(edit_credential_password, 2, 1, 1, 1);


        gridLayout->addWidget(group_credential, 3, 0, 1, 3);

        button_add_credential = new QToolButton(tab);
        button_add_credential->setObjectName(QString::fromUtf8("button_add_credential"));

        gridLayout->addWidget(button_add_credential, 2, 1, 1, 1);

        button_remove_credential = new QToolButton(tab);
        button_remove_credential->setObjectName(QString::fromUtf8("button_remove_credential"));

        gridLayout->addWidget(button_remove_credential, 2, 2, 1, 1);

        frame2_editAccounts->addTab(tab, QString());
        tab_2 = new QWidget();
        tab_2->setObjectName(QString::fromUtf8("tab_2"));
        gridLayout_3 = new QGridLayout(tab_2);
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        scrollArea = new QScrollArea(tab_2);
        scrollArea->setObjectName(QString::fromUtf8("scrollArea"));
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 536, 758));
        gridLayout_5 = new QGridLayout(scrollAreaWidgetContents);
        gridLayout_5->setObjectName(QString::fromUtf8("gridLayout_5"));
        label_tls_info = new QLabel(scrollAreaWidgetContents);
        label_tls_info->setObjectName(QString::fromUtf8("label_tls_info"));
        QSizePolicy sizePolicy8(QSizePolicy::Minimum, QSizePolicy::Minimum);
        sizePolicy8.setHorizontalStretch(0);
        sizePolicy8.setVerticalStretch(0);
        sizePolicy8.setHeightForWidth(label_tls_info->sizePolicy().hasHeightForWidth());
        label_tls_info->setSizePolicy(sizePolicy8);
        label_tls_info->setWordWrap(true);

        gridLayout_5->addWidget(label_tls_info, 2, 0, 1, 2);

        group_security_tls = new QGroupBox(scrollAreaWidgetContents);
        group_security_tls->setObjectName(QString::fromUtf8("group_security_tls"));
        sizePolicy1.setHeightForWidth(group_security_tls->sizePolicy().hasHeightForWidth());
        group_security_tls->setSizePolicy(sizePolicy1);
        group_security_tls->setCheckable(true);
        group_security_tls->setChecked(false);
        gridLayout_4 = new QGridLayout(group_security_tls);
        gridLayout_4->setObjectName(QString::fromUtf8("gridLayout_4"));
        label_tls_listener = new QLabel(group_security_tls);
        label_tls_listener->setObjectName(QString::fromUtf8("label_tls_listener"));

        gridLayout_4->addWidget(label_tls_listener, 0, 0, 1, 1);

        spinbox_tls_listener = new KIntSpinBox(group_security_tls);
        spinbox_tls_listener->setObjectName(QString::fromUtf8("spinbox_tls_listener"));
        spinbox_tls_listener->setMaximum(65535);

        gridLayout_4->addWidget(spinbox_tls_listener, 0, 1, 1, 3);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_4->addItem(horizontalSpacer_4, 0, 4, 1, 5);

        label_tls_authority = new QLabel(group_security_tls);
        label_tls_authority->setObjectName(QString::fromUtf8("label_tls_authority"));

        gridLayout_4->addWidget(label_tls_authority, 1, 0, 1, 1);

        file_tls_authority = new KUrlRequester(group_security_tls);
        file_tls_authority->setObjectName(QString::fromUtf8("file_tls_authority"));

        gridLayout_4->addWidget(file_tls_authority, 1, 1, 1, 8);

        label_tls_endpoint = new QLabel(group_security_tls);
        label_tls_endpoint->setObjectName(QString::fromUtf8("label_tls_endpoint"));

        gridLayout_4->addWidget(label_tls_endpoint, 2, 0, 1, 1);

        file_tls_endpoint = new KUrlRequester(group_security_tls);
        file_tls_endpoint->setObjectName(QString::fromUtf8("file_tls_endpoint"));

        gridLayout_4->addWidget(file_tls_endpoint, 2, 1, 1, 8);

        label_tls_private_key = new QLabel(group_security_tls);
        label_tls_private_key->setObjectName(QString::fromUtf8("label_tls_private_key"));

        gridLayout_4->addWidget(label_tls_private_key, 3, 0, 1, 1);

        file_tls_private_key = new KUrlRequester(group_security_tls);
        file_tls_private_key->setObjectName(QString::fromUtf8("file_tls_private_key"));

        gridLayout_4->addWidget(file_tls_private_key, 3, 1, 1, 8);

        label_tls_private_key_password = new QLabel(group_security_tls);
        label_tls_private_key_password->setObjectName(QString::fromUtf8("label_tls_private_key_password"));

        gridLayout_4->addWidget(label_tls_private_key_password, 4, 0, 1, 1);

        edit_tls_private_key_password = new KLineEdit(group_security_tls);
        edit_tls_private_key_password->setObjectName(QString::fromUtf8("edit_tls_private_key_password"));

        gridLayout_4->addWidget(edit_tls_private_key_password, 4, 1, 1, 8);

        label_tls_method = new QLabel(group_security_tls);
        label_tls_method->setObjectName(QString::fromUtf8("label_tls_method"));

        gridLayout_4->addWidget(label_tls_method, 5, 0, 1, 1);

        combo_tls_method = new QComboBox(group_security_tls);
        combo_tls_method->setObjectName(QString::fromUtf8("combo_tls_method"));

        gridLayout_4->addWidget(combo_tls_method, 5, 1, 1, 3);

        horizontalSpacer_5 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_4->addItem(horizontalSpacer_5, 5, 4, 1, 5);

        label_tls_cipher = new QLabel(group_security_tls);
        label_tls_cipher->setObjectName(QString::fromUtf8("label_tls_cipher"));

        gridLayout_4->addWidget(label_tls_cipher, 6, 0, 1, 1);

        edit_tls_cipher = new KLineEdit(group_security_tls);
        edit_tls_cipher->setObjectName(QString::fromUtf8("edit_tls_cipher"));

        gridLayout_4->addWidget(edit_tls_cipher, 6, 1, 1, 8);

        label_tls_outgoing = new QLabel(group_security_tls);
        label_tls_outgoing->setObjectName(QString::fromUtf8("label_tls_outgoing"));

        gridLayout_4->addWidget(label_tls_outgoing, 7, 0, 1, 1);

        edit_tls_outgoing = new KLineEdit(group_security_tls);
        edit_tls_outgoing->setObjectName(QString::fromUtf8("edit_tls_outgoing"));

        gridLayout_4->addWidget(edit_tls_outgoing, 7, 1, 1, 8);

        label_tls_timeout = new QLabel(group_security_tls);
        label_tls_timeout->setObjectName(QString::fromUtf8("label_tls_timeout"));

        gridLayout_4->addWidget(label_tls_timeout, 8, 0, 1, 1);

        spinbox_tls_timeout_sec = new KIntSpinBox(group_security_tls);
        spinbox_tls_timeout_sec->setObjectName(QString::fromUtf8("spinbox_tls_timeout_sec"));
        sizePolicy3.setHeightForWidth(spinbox_tls_timeout_sec->sizePolicy().hasHeightForWidth());
        spinbox_tls_timeout_sec->setSizePolicy(sizePolicy3);
        spinbox_tls_timeout_sec->setMinimumSize(QSize(50, 0));

        gridLayout_4->addWidget(spinbox_tls_timeout_sec, 8, 1, 1, 1);

        check_tls_incoming = new QCheckBox(group_security_tls);
        check_tls_incoming->setObjectName(QString::fromUtf8("check_tls_incoming"));

        gridLayout_4->addWidget(check_tls_incoming, 9, 0, 1, 5);

        check_tls_answer = new QCheckBox(group_security_tls);
        check_tls_answer->setObjectName(QString::fromUtf8("check_tls_answer"));

        gridLayout_4->addWidget(check_tls_answer, 10, 0, 1, 5);

        check_tls_requier_cert = new QCheckBox(group_security_tls);
        check_tls_requier_cert->setObjectName(QString::fromUtf8("check_tls_requier_cert"));

        gridLayout_4->addWidget(check_tls_requier_cert, 11, 0, 1, 5);

        label_tls_details = new QLabel(group_security_tls);
        label_tls_details->setObjectName(QString::fromUtf8("label_tls_details"));

        gridLayout_4->addWidget(label_tls_details, 12, 0, 1, 1);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_4->addItem(verticalSpacer, 13, 0, 1, 1);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_4->addItem(horizontalSpacer_3, 8, 4, 1, 5);

        spinbox_tls_timeout_msec = new KIntSpinBox(group_security_tls);
        spinbox_tls_timeout_msec->setObjectName(QString::fromUtf8("spinbox_tls_timeout_msec"));
        sizePolicy3.setHeightForWidth(spinbox_tls_timeout_msec->sizePolicy().hasHeightForWidth());
        spinbox_tls_timeout_msec->setSizePolicy(sizePolicy3);
        spinbox_tls_timeout_msec->setMinimumSize(QSize(50, 0));

        gridLayout_4->addWidget(spinbox_tls_timeout_msec, 8, 3, 1, 1);

        label_timeout2 = new QLabel(group_security_tls);
        label_timeout2->setObjectName(QString::fromUtf8("label_timeout2"));
        sizePolicy8.setHeightForWidth(label_timeout2->sizePolicy().hasHeightForWidth());
        label_timeout2->setSizePolicy(sizePolicy8);
        label_timeout2->setMinimumSize(QSize(10, 0));
        label_timeout2->setMaximumSize(QSize(10, 16777215));

        gridLayout_4->addWidget(label_timeout2, 8, 2, 1, 1);


        gridLayout_5->addWidget(group_security_tls, 3, 0, 1, 2);

        groupbox_STRP_keyexchange = new QGroupBox(scrollAreaWidgetContents);
        groupbox_STRP_keyexchange->setObjectName(QString::fromUtf8("groupbox_STRP_keyexchange"));
        verticalLayout_2 = new QVBoxLayout(groupbox_STRP_keyexchange);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        combo_security_STRP = new QComboBox(groupbox_STRP_keyexchange);
        combo_security_STRP->setObjectName(QString::fromUtf8("combo_security_STRP"));
        QSizePolicy sizePolicy9(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy9.setHorizontalStretch(0);
        sizePolicy9.setVerticalStretch(0);
        sizePolicy9.setHeightForWidth(combo_security_STRP->sizePolicy().hasHeightForWidth());
        combo_security_STRP->setSizePolicy(sizePolicy9);

        verticalLayout_2->addWidget(combo_security_STRP);

        checkbox_ZTRP_send_hello = new QCheckBox(groupbox_STRP_keyexchange);
        checkbox_ZTRP_send_hello->setObjectName(QString::fromUtf8("checkbox_ZTRP_send_hello"));

        verticalLayout_2->addWidget(checkbox_ZTRP_send_hello);

        checkbox_ZRTP_warn_supported = new QCheckBox(groupbox_STRP_keyexchange);
        checkbox_ZRTP_warn_supported->setObjectName(QString::fromUtf8("checkbox_ZRTP_warn_supported"));

        verticalLayout_2->addWidget(checkbox_ZRTP_warn_supported);

        checkbox_ZRTP_Ask_user = new QCheckBox(groupbox_STRP_keyexchange);
        checkbox_ZRTP_Ask_user->setObjectName(QString::fromUtf8("checkbox_ZRTP_Ask_user"));

        verticalLayout_2->addWidget(checkbox_ZRTP_Ask_user);

        checkbox_ZRTP_display_SAS = new QCheckBox(groupbox_STRP_keyexchange);
        checkbox_ZRTP_display_SAS->setObjectName(QString::fromUtf8("checkbox_ZRTP_display_SAS"));

        verticalLayout_2->addWidget(checkbox_ZRTP_display_SAS);

        checkbox_SDES_fallback_rtp = new QCheckBox(groupbox_STRP_keyexchange);
        checkbox_SDES_fallback_rtp->setObjectName(QString::fromUtf8("checkbox_SDES_fallback_rtp"));

        verticalLayout_2->addWidget(checkbox_SDES_fallback_rtp);


        gridLayout_5->addWidget(groupbox_STRP_keyexchange, 0, 0, 1, 2);

        scrollArea->setWidget(scrollAreaWidgetContents);

        gridLayout_3->addWidget(scrollArea, 0, 0, 1, 1);

        frame2_editAccounts->addTab(tab_2, QString());

        horizontalLayout_3->addWidget(frame2_editAccounts);


        verticalLayout->addWidget(widget1_configAccounts);

#ifndef UI_QT_NO_SHORTCUT
        label1_alias->setBuddy(edit1_alias);
        label2_protocol->setBuddy(edit2_protocol);
        label3_server->setBuddy(edit3_server);
        label4_user->setBuddy(edit4_user);
        label5_password->setBuddy(edit5_password);
        label6_mailbox->setBuddy(edit6_mailbox);
        label_regExpire->setBuddy(spinbox_regExpire);
#endif // QT_NO_SHORTCUT

        retranslateUi(DlgAccountsBase);
        QObject::connect(checkbox_stun, SIGNAL(toggled(bool)), line_stun, SLOT(setEnabled(bool)));

        frame2_editAccounts->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(DlgAccountsBase);
    } // setupUi

    void retranslateUi(QWidget *DlgAccountsBase)
    {
#ifndef UI_QT_NO_WHATSTHIS
        listWidget_accountList->setWhatsThis(tr2i18n("By default, when you place a call, sflphone will use the first account in this list which is \"registered\". Change the order of the accounts using the \"Up\" and \"Down\" arrows. Enable/disable them by checking/unchecking them on the left of the item. Add or remove some with \"Plus\" and \"Sub\" buttons. Edit the selected account with the form on the right.", 0));
#endif // QT_NO_WHATSTHIS
        groupBox_accountListHandle->setTitle(QString());
#ifndef UI_QT_NO_TOOLTIP
        button_accountRemove->setToolTip(tr2i18n("Remove the selected account", 0));
#endif // QT_NO_TOOLTIP
#ifndef UI_QT_NO_WHATSTHIS
        button_accountRemove->setWhatsThis(tr2i18n("This button will remove the selected account in the list below. Be sure you really don't need it anymore. If you think you might use it again later, just uncheck it.", 0));
#endif // QT_NO_WHATSTHIS
#ifndef UI_QT_NO_ACCESSIBILITY
        button_accountRemove->setAccessibleDescription(QString());
#endif // QT_NO_ACCESSIBILITY
        button_accountRemove->setText(tr2i18n("Remove", 0));
#ifndef UI_QT_NO_TOOLTIP
        button_accountAdd->setToolTip(tr2i18n("Add a new account", 0));
#endif // QT_NO_TOOLTIP
#ifndef UI_QT_NO_WHATSTHIS
        button_accountAdd->setWhatsThis(tr2i18n("This button enables you to initialize a new account. You will then be able to edit it using the form on the right.", 0));
#endif // QT_NO_WHATSTHIS
        button_accountAdd->setText(tr2i18n("Add", 0));
#ifndef UI_QT_NO_TOOLTIP
        button_accountDown->setToolTip(tr2i18n("Get this account down", 0));
#endif // QT_NO_TOOLTIP
#ifndef UI_QT_NO_WHATSTHIS
        button_accountDown->setWhatsThis(tr2i18n("By default, when you place a call, sflphone will use the first account in this list which is \"registered\". Change the order of the accounts using the \"Up\" and \"Down\" arrows.", 0));
#endif // QT_NO_WHATSTHIS
        button_accountDown->setText(tr2i18n("Down", 0));
#ifndef UI_QT_NO_TOOLTIP
        button_accountUp->setToolTip(tr2i18n("Get this account up", 0));
#endif // QT_NO_TOOLTIP
#ifndef UI_QT_NO_WHATSTHIS
        button_accountUp->setWhatsThis(tr2i18n("By default, when you place a call, sflphone will use the first account in this list which is \"registered\". Change the order of the accounts using the \"Up\" and \"Down\" arrows.", 0));
#endif // QT_NO_WHATSTHIS
        button_accountUp->setText(tr2i18n("Up", 0));
        label1_alias->setText(tr2i18n("Alias", 0));
        label2_protocol->setText(tr2i18n("Protocol", 0));
        edit2_protocol->clear();
        edit2_protocol->insertItems(0, QStringList()
         << tr2i18n("SIP", 0)
         << tr2i18n("IAX", 0)
        );
        label3_server->setText(tr2i18n("Server", 0));
        label4_user->setText(tr2i18n("Username", 0));
        label5_password->setText(tr2i18n("Password", 0));
        label6_mailbox->setText(tr2i18n("Voicemail", 0));
        label7_state->setText(tr2i18n("Status", 0));
        edit7_state->setText(QString());
        frame2_editAccounts->setTabText(frame2_editAccounts->indexOf(tab_basic), tr2i18n("Basic", 0));
        groupBox->setTitle(tr2i18n("Resgistration", 0));
        label_regExpire->setText(tr2i18n("Registration expire", 0));
        checkBox_conformRFC->setText(tr2i18n("Conform to RFC 3263", 0));
        groupBox_2->setTitle(tr2i18n("Network Interface", 0));
        label_ni_local_address->setText(tr2i18n("Local address", 0));
        label_ni_local_port->setText(tr2i18n("Local port", 0));
        groupBox_3->setTitle(tr2i18n("Published address", 0));
        radioButton_pa_same_as_local->setText(tr2i18n("Same as local parameters", 0));
        radioButton_pa_custom->setText(tr2i18n("Set published address and port", 0));
        label_published_address->setText(tr2i18n("Published address", 0));
        label_pa_published_port->setText(tr2i18n("Published port", 0));
        frame2_editAccounts->setTabText(frame2_editAccounts->indexOf(tab_advanced), tr2i18n("Advanced", 0));
        label_commonSettings->setText(tr2i18n("Stun parameters will be applied on each SIP account created.", 0));
        checkbox_stun->setText(tr2i18n("Enable Stun", 0));
        line_stun->setClickMessage(tr2i18n("choose Stun server (example : stunserver.org)", 0));
        frame2_editAccounts->setTabText(frame2_editAccounts->indexOf(tab_stun), tr2i18n("Stun", 0));
        label_frequency->setText(tr2i18n("Frequency: ", 0));
        label_frequency_value->setText(tr2i18n("-", 0));
        label_bitrate->setText(tr2i18n("Bitrate:", 0));
        label_bitrate_value->setText(tr2i18n("-", 0));
        label_bandwidth->setText(tr2i18n("Bandwidth: ", 0));
        label_bandwidth_value->setText(tr2i18n("-", 0));
        frame2_editAccounts->setTabText(frame2_editAccounts->indexOf(tab_codec), tr2i18n("Codecs", 0));
        group_credential->setTitle(tr2i18n("Details", 0));
        label_credential_realm->setText(tr2i18n("Realm", 0));
        labe_credential_auth->setText(tr2i18n("Auth. name", 0));
        label_credential_password->setText(tr2i18n("Password", 0));
        button_add_credential->setText(tr2i18n("Add", 0));
        button_remove_credential->setText(tr2i18n("Remove", 0));
        frame2_editAccounts->setTabText(frame2_editAccounts->indexOf(tab), tr2i18n("Credential", 0));
        label_tls_info->setText(tr2i18n("TLS transport can be used along with UDP for those calls that would require secure sip transactions (aka SIPS). You can configure a different TLS transport for each account. However each of them will run on a dedicated port, different one from each other.", 0));
        group_security_tls->setTitle(tr2i18n("Enable TLS", 0));
        label_tls_listener->setText(tr2i18n("Global TLS listener*", 0));
        label_tls_authority->setText(tr2i18n("Authority certificate list", 0));
        label_tls_endpoint->setText(tr2i18n("Public endpoint certificate", 0));
        label_tls_private_key->setText(tr2i18n("Private key", 0));
        label_tls_private_key_password->setText(tr2i18n("Private key password", 0));
        label_tls_method->setText(tr2i18n("TLS protocol method", 0));
        combo_tls_method->clear();
        combo_tls_method->insertItems(0, QStringList()
         << tr2i18n("Default", 0)
         << tr2i18n("TLSv1", 0)
         << tr2i18n("SSLv2", 0)
         << tr2i18n("SSLv3", 0)
         << tr2i18n("SSLv23", 0)
        );
        label_tls_cipher->setText(tr2i18n("TLS cipher list", 0));
        label_tls_outgoing->setText(tr2i18n("Outgoing TLS server name", 0));
        label_tls_timeout->setText(tr2i18n("Negotiation timeout (s:ms)", 0));
        check_tls_incoming->setText(tr2i18n("Verify incoming certificates (server side)", 0));
        check_tls_answer->setText(tr2i18n("Verify answer certificates (client side)", 0));
        check_tls_requier_cert->setText(tr2i18n("Require a certificate for incoming TLS connections", 0));
        label_tls_details->setText(tr2i18n("*Apply to all accounts", 0));
        label_timeout2->setText(tr2i18n("<center>:</center>", 0));
        groupbox_STRP_keyexchange->setTitle(tr2i18n("SRTP key exchange", 0));
        combo_security_STRP->clear();
        combo_security_STRP->insertItems(0, QStringList()
         << tr2i18n("Disabled", 0)
         << tr2i18n("ZRTP", 0)
         << tr2i18n("SDES", 0)
        );
        checkbox_ZTRP_send_hello->setText(tr2i18n("Send Hello Hash in SDP", 0));
        checkbox_ZRTP_warn_supported->setText(tr2i18n("Ask user to confirm SAS", 0));
        checkbox_ZRTP_Ask_user->setText(tr2i18n("Warn if ZRTP is not supported", 0));
        checkbox_ZRTP_display_SAS->setText(tr2i18n("Display SAS once for hold events", 0));
        checkbox_SDES_fallback_rtp->setText(tr2i18n("Fallback on RTP on SDES failure", 0));
        frame2_editAccounts->setTabText(frame2_editAccounts->indexOf(tab_2), tr2i18n("Security", 0));
        Q_UNUSED(DlgAccountsBase);
    } // retranslateUi

};

namespace Ui {
    class DlgAccountsBase: public Ui_DlgAccountsBase {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DLGACCOUNTSBASE_H

