/********************************************************************************
** Form generated from reading ui file 'ConfigDialog.ui'
**
** Created: Tue Feb 17 11:57:42 2009
**      by: Qt User Interface Compiler version 4.4.3
**
** WARNING! All changes made in this file will be lost when recompiling ui file!
********************************************************************************/

#ifndef UI_CONFIGDIALOG_H
#define UI_CONFIGDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDialog>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QFormLayout>
#include <QtGui/QFrame>
#include <QtGui/QGridLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QListWidget>
#include <QtGui/QPushButton>
#include <QtGui/QSlider>
#include <QtGui/QSpinBox>
#include <QtGui/QStackedWidget>
#include <QtGui/QTableView>
#include <QtGui/QToolButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ConfigurationDialog
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayoutDialog;
    QListWidget *listOptions;
    QFrame *lineOptions;
    QStackedWidget *stackedWidgetOptions;
    QWidget *page_General;
    QVBoxLayout *verticalLayout_18;
    QLabel *label_ConfGeneral;
    QFrame *line_ConfGeneral;
    QGroupBox *groupBox1_Historique;
    QVBoxLayout *verticalLayout_19;
    QWidget *widget_CapaciteHist;
    QHBoxLayout *horizontalLayout_10;
    QLabel *label_Capacite;
    QSlider *horizontalSlider;
    QSpinBox *spinBox_CapaciteHist;
    QToolButton *toolButtonEffacerHist;
    QGroupBox *groupBox2_Connexion;
    QFormLayout *formLayout_12;
    QLabel *label_PortSIP;
    QSpinBox *spinBox_PortSIP;
    QWidget *widget_RemplissageConfGeneral;
    QWidget *page_Affichage;
    QVBoxLayout *verticalLayout_9;
    QLabel *label_ConfAffichage;
    QFrame *line_ConfAffichage;
    QFrame *frameAffichage;
    QVBoxLayout *verticalLayout_8;
    QLabel *label1_Notifications;
    QWidget *widget1_Notifications;
    QHBoxLayout *horizontalLayout_5;
    QCheckBox *checkBox1_NotifAppels;
    QCheckBox *checkBox2_NotifMessages;
    QLabel *label2_FenetrePrincipale;
    QWidget *widget_FenetrePrincipale;
    QHBoxLayout *horizontalLayout_6;
    QCheckBox *checkBox1_FenDemarrage;
    QCheckBox *checkBox2_FenAppel;
    QWidget *widget_5;
    QWidget *page_Comptes;
    QVBoxLayout *verticalLayout_2;
    QLabel *label_ConfComptes;
    QFrame *line_ConfComptes;
    QWidget *widget1_ConfComptes;
    QHBoxLayout *horizontalLayout_3;
    QFrame *frame1_ListeComptes;
    QVBoxLayout *verticalLayout_6;
    QListWidget *listWidgetComptes;
    QGroupBox *groupBoxGestionComptes;
    QHBoxLayout *horizontalLayout_2;
    QToolButton *buttonSupprimerCompte;
    QToolButton *buttonNouveauCompte;
    QFrame *frame2_EditComptes;
    QFormLayout *formLayout_2;
    QLabel *label1_Alias;
    QLineEdit *edit1_Alias;
    QLabel *label2_Protocole;
    QLabel *label3_Serveur;
    QLabel *label4_Usager;
    QLabel *label5_Mdp;
    QLabel *label6_BoiteVocale;
    QLineEdit *edit3_Serveur;
    QLineEdit *edit4_Usager;
    QLineEdit *edit5_Mdp;
    QLineEdit *edit6_BoiteVocale;
    QComboBox *edit2_Protocole;
    QGroupBox *groupBox_ConfComptesCommuns;
    QVBoxLayout *verticalLayout_10;
    QLabel *label_ConfComptesCommus;
    QFormLayout *formLayoutConfComptesCommus;
    QCheckBox *checkBoxStun;
    QLineEdit *lineEdit_Stun;
    QWidget *page_Audio;
    QVBoxLayout *verticalLayout_5;
    QLabel *label_ConfAudio;
    QFrame *line_ConfAudio;
    QGroupBox *groupBox1_Audio;
    QFormLayout *formLayout_3;
    QLabel *label_Interface;
    QComboBox *comboBox_Interface;
    QCheckBox *checkBox_Sonneries;
    QGroupBox *groupBox2_Codecs;
    QGridLayout *gridLayout;
    QTableView *tableViewCodecs;
    QVBoxLayout *verticalLayout_OrdreCodecs;
    QToolButton *toolButton_MonterCodec;
    QToolButton *toolButton_DescendreCodec;
    QStackedWidget *stackedWidget_ParametresSpecif;
    QWidget *page1_Alsa;
    QVBoxLayout *verticalLayout_20;
    QGroupBox *groupBox_Alsa;
    QFormLayout *formLayout_4;
    QComboBox *comboBox2_Entree;
    QLabel *label2_Entree;
    QLabel *label3_Sortie;
    QComboBox *comboBox3_Sortie;
    QLabel *label1_GreffonAlsa;
    QComboBox *comboBox1_GreffonAlsa;
    QWidget *page2_PulseAudio;
    QVBoxLayout *verticalLayout_7;
    QGroupBox *groupBox_PulseAudio;
    QFormLayout *formLayout_11;
    QCheckBox *checkBox_ModifVolumeApps;
    QPushButton *pushButton;
    QFrame *lineDialog;
    QDialogButtonBox *buttonBoxDialog;

    void setupUi(QDialog *ConfigurationDialog)
    {
    if (ConfigurationDialog->objectName().isEmpty())
        ConfigurationDialog->setObjectName(QString::fromUtf8("ConfigurationDialog"));
    ConfigurationDialog->resize(504, 392);
    ConfigurationDialog->setMinimumSize(QSize(0, 0));
    verticalLayout = new QVBoxLayout(ConfigurationDialog);
    verticalLayout->setSpacing(4);
    verticalLayout->setMargin(1);
    verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
    horizontalLayoutDialog = new QHBoxLayout();
    horizontalLayoutDialog->setSpacing(4);
    horizontalLayoutDialog->setObjectName(QString::fromUtf8("horizontalLayoutDialog"));
    horizontalLayoutDialog->setSizeConstraint(QLayout::SetDefaultConstraint);
    listOptions = new QListWidget(ConfigurationDialog);
    QIcon icon;
    icon.addPixmap(QPixmap(QString::fromUtf8(":/Images/sflphone.png")), QIcon::Normal, QIcon::Off);
    QListWidgetItem *__listItem = new QListWidgetItem(listOptions);
    __listItem->setIcon(icon);
    QListWidgetItem *__listItem1 = new QListWidgetItem(listOptions);
    __listItem1->setIcon(icon);
    QIcon icon1;
    icon1.addPixmap(QPixmap(QString::fromUtf8(":/Images/stock_person.svg")), QIcon::Normal, QIcon::Off);
    QListWidgetItem *__listItem2 = new QListWidgetItem(listOptions);
    __listItem2->setIcon(icon1);
    QIcon icon2;
    icon2.addPixmap(QPixmap(QString::fromUtf8(":/Images/icon_volume_off.svg")), QIcon::Normal, QIcon::Off);
    QListWidgetItem *__listItem3 = new QListWidgetItem(listOptions);
    __listItem3->setIcon(icon2);
    listOptions->setObjectName(QString::fromUtf8("listOptions"));
    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(listOptions->sizePolicy().hasHeightForWidth());
    listOptions->setSizePolicy(sizePolicy);
    listOptions->setMinimumSize(QSize(100, 300));
    listOptions->setMaximumSize(QSize(100, 16777215));
    listOptions->setSizeIncrement(QSize(0, 0));
    listOptions->setMouseTracking(false);
    listOptions->setContextMenuPolicy(Qt::DefaultContextMenu);
    listOptions->setAcceptDrops(false);
    listOptions->setLayoutDirection(Qt::LeftToRight);
    listOptions->setAutoFillBackground(true);
    listOptions->setAutoScrollMargin(16);
    listOptions->setEditTriggers(QAbstractItemView::AllEditTriggers);
    listOptions->setDragEnabled(true);
    listOptions->setIconSize(QSize(200, 200));
    listOptions->setTextElideMode(Qt::ElideMiddle);
    listOptions->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    listOptions->setHorizontalScrollMode(QAbstractItemView::ScrollPerItem);
    listOptions->setMovement(QListView::Static);
    listOptions->setFlow(QListView::TopToBottom);
    listOptions->setResizeMode(QListView::Adjust);
    listOptions->setLayoutMode(QListView::SinglePass);
    listOptions->setViewMode(QListView::IconMode);
    listOptions->setModelColumn(0);
    listOptions->setUniformItemSizes(false);
    listOptions->setWordWrap(false);
    listOptions->setSortingEnabled(false);

    horizontalLayoutDialog->addWidget(listOptions);

    lineOptions = new QFrame(ConfigurationDialog);
    lineOptions->setObjectName(QString::fromUtf8("lineOptions"));
    lineOptions->setFrameShape(QFrame::VLine);
    lineOptions->setFrameShadow(QFrame::Sunken);

    horizontalLayoutDialog->addWidget(lineOptions);

    stackedWidgetOptions = new QStackedWidget(ConfigurationDialog);
    stackedWidgetOptions->setObjectName(QString::fromUtf8("stackedWidgetOptions"));
    page_General = new QWidget();
    page_General->setObjectName(QString::fromUtf8("page_General"));
    verticalLayout_18 = new QVBoxLayout(page_General);
    verticalLayout_18->setSpacing(4);
    verticalLayout_18->setMargin(4);
    verticalLayout_18->setObjectName(QString::fromUtf8("verticalLayout_18"));
    verticalLayout_18->setContentsMargins(4, -1, -1, -1);
    label_ConfGeneral = new QLabel(page_General);
    label_ConfGeneral->setObjectName(QString::fromUtf8("label_ConfGeneral"));

    verticalLayout_18->addWidget(label_ConfGeneral);

    line_ConfGeneral = new QFrame(page_General);
    line_ConfGeneral->setObjectName(QString::fromUtf8("line_ConfGeneral"));
    line_ConfGeneral->setFrameShape(QFrame::HLine);
    line_ConfGeneral->setFrameShadow(QFrame::Sunken);

    verticalLayout_18->addWidget(line_ConfGeneral);

    groupBox1_Historique = new QGroupBox(page_General);
    groupBox1_Historique->setObjectName(QString::fromUtf8("groupBox1_Historique"));
    QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(groupBox1_Historique->sizePolicy().hasHeightForWidth());
    groupBox1_Historique->setSizePolicy(sizePolicy1);
    verticalLayout_19 = new QVBoxLayout(groupBox1_Historique);
    verticalLayout_19->setSpacing(4);
    verticalLayout_19->setMargin(4);
    verticalLayout_19->setObjectName(QString::fromUtf8("verticalLayout_19"));
    widget_CapaciteHist = new QWidget(groupBox1_Historique);
    widget_CapaciteHist->setObjectName(QString::fromUtf8("widget_CapaciteHist"));
    horizontalLayout_10 = new QHBoxLayout(widget_CapaciteHist);
    horizontalLayout_10->setSpacing(4);
    horizontalLayout_10->setMargin(4);
    horizontalLayout_10->setObjectName(QString::fromUtf8("horizontalLayout_10"));
    label_Capacite = new QLabel(widget_CapaciteHist);
    label_Capacite->setObjectName(QString::fromUtf8("label_Capacite"));

    horizontalLayout_10->addWidget(label_Capacite);

    horizontalSlider = new QSlider(widget_CapaciteHist);
    horizontalSlider->setObjectName(QString::fromUtf8("horizontalSlider"));
    horizontalSlider->setMaximum(100);
    horizontalSlider->setOrientation(Qt::Horizontal);

    horizontalLayout_10->addWidget(horizontalSlider);

    spinBox_CapaciteHist = new QSpinBox(widget_CapaciteHist);
    spinBox_CapaciteHist->setObjectName(QString::fromUtf8("spinBox_CapaciteHist"));

    horizontalLayout_10->addWidget(spinBox_CapaciteHist);


    verticalLayout_19->addWidget(widget_CapaciteHist);

    toolButtonEffacerHist = new QToolButton(groupBox1_Historique);
    toolButtonEffacerHist->setObjectName(QString::fromUtf8("toolButtonEffacerHist"));

    verticalLayout_19->addWidget(toolButtonEffacerHist);


    verticalLayout_18->addWidget(groupBox1_Historique);

    groupBox2_Connexion = new QGroupBox(page_General);
    groupBox2_Connexion->setObjectName(QString::fromUtf8("groupBox2_Connexion"));
    sizePolicy1.setHeightForWidth(groupBox2_Connexion->sizePolicy().hasHeightForWidth());
    groupBox2_Connexion->setSizePolicy(sizePolicy1);
    formLayout_12 = new QFormLayout(groupBox2_Connexion);
    formLayout_12->setSpacing(4);
    formLayout_12->setMargin(4);
    formLayout_12->setObjectName(QString::fromUtf8("formLayout_12"));
    label_PortSIP = new QLabel(groupBox2_Connexion);
    label_PortSIP->setObjectName(QString::fromUtf8("label_PortSIP"));

    formLayout_12->setWidget(0, QFormLayout::LabelRole, label_PortSIP);

    spinBox_PortSIP = new QSpinBox(groupBox2_Connexion);
    spinBox_PortSIP->setObjectName(QString::fromUtf8("spinBox_PortSIP"));

    formLayout_12->setWidget(0, QFormLayout::FieldRole, spinBox_PortSIP);


    verticalLayout_18->addWidget(groupBox2_Connexion);

    widget_RemplissageConfGeneral = new QWidget(page_General);
    widget_RemplissageConfGeneral->setObjectName(QString::fromUtf8("widget_RemplissageConfGeneral"));
    sizePolicy.setHeightForWidth(widget_RemplissageConfGeneral->sizePolicy().hasHeightForWidth());
    widget_RemplissageConfGeneral->setSizePolicy(sizePolicy);

    verticalLayout_18->addWidget(widget_RemplissageConfGeneral);

    stackedWidgetOptions->addWidget(page_General);
    page_Affichage = new QWidget();
    page_Affichage->setObjectName(QString::fromUtf8("page_Affichage"));
    verticalLayout_9 = new QVBoxLayout(page_Affichage);
    verticalLayout_9->setSpacing(4);
    verticalLayout_9->setMargin(4);
    verticalLayout_9->setObjectName(QString::fromUtf8("verticalLayout_9"));
    label_ConfAffichage = new QLabel(page_Affichage);
    label_ConfAffichage->setObjectName(QString::fromUtf8("label_ConfAffichage"));
    sizePolicy1.setHeightForWidth(label_ConfAffichage->sizePolicy().hasHeightForWidth());
    label_ConfAffichage->setSizePolicy(sizePolicy1);

    verticalLayout_9->addWidget(label_ConfAffichage);

    line_ConfAffichage = new QFrame(page_Affichage);
    line_ConfAffichage->setObjectName(QString::fromUtf8("line_ConfAffichage"));
    line_ConfAffichage->setFrameShape(QFrame::HLine);
    line_ConfAffichage->setFrameShadow(QFrame::Sunken);

    verticalLayout_9->addWidget(line_ConfAffichage);

    frameAffichage = new QFrame(page_Affichage);
    frameAffichage->setObjectName(QString::fromUtf8("frameAffichage"));
    QSizePolicy sizePolicy2(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(frameAffichage->sizePolicy().hasHeightForWidth());
    frameAffichage->setSizePolicy(sizePolicy2);
    frameAffichage->setFrameShape(QFrame::StyledPanel);
    frameAffichage->setFrameShadow(QFrame::Raised);
    verticalLayout_8 = new QVBoxLayout(frameAffichage);
    verticalLayout_8->setSpacing(4);
    verticalLayout_8->setMargin(4);
    verticalLayout_8->setObjectName(QString::fromUtf8("verticalLayout_8"));
    label1_Notifications = new QLabel(frameAffichage);
    label1_Notifications->setObjectName(QString::fromUtf8("label1_Notifications"));
    sizePolicy1.setHeightForWidth(label1_Notifications->sizePolicy().hasHeightForWidth());
    label1_Notifications->setSizePolicy(sizePolicy1);

    verticalLayout_8->addWidget(label1_Notifications);

    widget1_Notifications = new QWidget(frameAffichage);
    widget1_Notifications->setObjectName(QString::fromUtf8("widget1_Notifications"));
    sizePolicy1.setHeightForWidth(widget1_Notifications->sizePolicy().hasHeightForWidth());
    widget1_Notifications->setSizePolicy(sizePolicy1);
    horizontalLayout_5 = new QHBoxLayout(widget1_Notifications);
    horizontalLayout_5->setSpacing(4);
    horizontalLayout_5->setMargin(4);
    horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
    checkBox1_NotifAppels = new QCheckBox(widget1_Notifications);
    checkBox1_NotifAppels->setObjectName(QString::fromUtf8("checkBox1_NotifAppels"));

    horizontalLayout_5->addWidget(checkBox1_NotifAppels);

    checkBox2_NotifMessages = new QCheckBox(widget1_Notifications);
    checkBox2_NotifMessages->setObjectName(QString::fromUtf8("checkBox2_NotifMessages"));

    horizontalLayout_5->addWidget(checkBox2_NotifMessages);


    verticalLayout_8->addWidget(widget1_Notifications);

    label2_FenetrePrincipale = new QLabel(frameAffichage);
    label2_FenetrePrincipale->setObjectName(QString::fromUtf8("label2_FenetrePrincipale"));
    sizePolicy1.setHeightForWidth(label2_FenetrePrincipale->sizePolicy().hasHeightForWidth());
    label2_FenetrePrincipale->setSizePolicy(sizePolicy1);

    verticalLayout_8->addWidget(label2_FenetrePrincipale);

    widget_FenetrePrincipale = new QWidget(frameAffichage);
    widget_FenetrePrincipale->setObjectName(QString::fromUtf8("widget_FenetrePrincipale"));
    sizePolicy1.setHeightForWidth(widget_FenetrePrincipale->sizePolicy().hasHeightForWidth());
    widget_FenetrePrincipale->setSizePolicy(sizePolicy1);
    horizontalLayout_6 = new QHBoxLayout(widget_FenetrePrincipale);
    horizontalLayout_6->setSpacing(4);
    horizontalLayout_6->setMargin(4);
    horizontalLayout_6->setObjectName(QString::fromUtf8("horizontalLayout_6"));
    checkBox1_FenDemarrage = new QCheckBox(widget_FenetrePrincipale);
    checkBox1_FenDemarrage->setObjectName(QString::fromUtf8("checkBox1_FenDemarrage"));

    horizontalLayout_6->addWidget(checkBox1_FenDemarrage);

    checkBox2_FenAppel = new QCheckBox(widget_FenetrePrincipale);
    checkBox2_FenAppel->setObjectName(QString::fromUtf8("checkBox2_FenAppel"));

    horizontalLayout_6->addWidget(checkBox2_FenAppel);


    verticalLayout_8->addWidget(widget_FenetrePrincipale);

    widget_5 = new QWidget(frameAffichage);
    widget_5->setObjectName(QString::fromUtf8("widget_5"));

    verticalLayout_8->addWidget(widget_5);


    verticalLayout_9->addWidget(frameAffichage);

    stackedWidgetOptions->addWidget(page_Affichage);
    page_Comptes = new QWidget();
    page_Comptes->setObjectName(QString::fromUtf8("page_Comptes"));
    verticalLayout_2 = new QVBoxLayout(page_Comptes);
    verticalLayout_2->setSpacing(4);
    verticalLayout_2->setMargin(4);
    verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
    label_ConfComptes = new QLabel(page_Comptes);
    label_ConfComptes->setObjectName(QString::fromUtf8("label_ConfComptes"));
    sizePolicy2.setHeightForWidth(label_ConfComptes->sizePolicy().hasHeightForWidth());
    label_ConfComptes->setSizePolicy(sizePolicy2);

    verticalLayout_2->addWidget(label_ConfComptes);

    line_ConfComptes = new QFrame(page_Comptes);
    line_ConfComptes->setObjectName(QString::fromUtf8("line_ConfComptes"));
    line_ConfComptes->setFrameShape(QFrame::HLine);
    line_ConfComptes->setFrameShadow(QFrame::Sunken);

    verticalLayout_2->addWidget(line_ConfComptes);

    widget1_ConfComptes = new QWidget(page_Comptes);
    widget1_ConfComptes->setObjectName(QString::fromUtf8("widget1_ConfComptes"));
    horizontalLayout_3 = new QHBoxLayout(widget1_ConfComptes);
    horizontalLayout_3->setSpacing(4);
    horizontalLayout_3->setMargin(4);
    horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
    frame1_ListeComptes = new QFrame(widget1_ConfComptes);
    frame1_ListeComptes->setObjectName(QString::fromUtf8("frame1_ListeComptes"));
    frame1_ListeComptes->setMinimumSize(QSize(0, 0));
    frame1_ListeComptes->setMaximumSize(QSize(16777215, 16777215));
    frame1_ListeComptes->setFrameShape(QFrame::StyledPanel);
    frame1_ListeComptes->setFrameShadow(QFrame::Raised);
    verticalLayout_6 = new QVBoxLayout(frame1_ListeComptes);
    verticalLayout_6->setSpacing(4);
    verticalLayout_6->setMargin(4);
    verticalLayout_6->setObjectName(QString::fromUtf8("verticalLayout_6"));
    listWidgetComptes = new QListWidget(frame1_ListeComptes);
    listWidgetComptes->setObjectName(QString::fromUtf8("listWidgetComptes"));
    QSizePolicy sizePolicy3(QSizePolicy::Fixed, QSizePolicy::Expanding);
    sizePolicy3.setHorizontalStretch(0);
    sizePolicy3.setVerticalStretch(0);
    sizePolicy3.setHeightForWidth(listWidgetComptes->sizePolicy().hasHeightForWidth());
    listWidgetComptes->setSizePolicy(sizePolicy3);
    listWidgetComptes->setMinimumSize(QSize(150, 0));
    listWidgetComptes->setMaximumSize(QSize(150, 16777215));
    listWidgetComptes->setDragEnabled(true);

    verticalLayout_6->addWidget(listWidgetComptes);

    groupBoxGestionComptes = new QGroupBox(frame1_ListeComptes);
    groupBoxGestionComptes->setObjectName(QString::fromUtf8("groupBoxGestionComptes"));
    sizePolicy2.setHeightForWidth(groupBoxGestionComptes->sizePolicy().hasHeightForWidth());
    groupBoxGestionComptes->setSizePolicy(sizePolicy2);
    groupBoxGestionComptes->setMaximumSize(QSize(16777215, 16777215));
    groupBoxGestionComptes->setLayoutDirection(Qt::RightToLeft);
    groupBoxGestionComptes->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    horizontalLayout_2 = new QHBoxLayout(groupBoxGestionComptes);
    horizontalLayout_2->setSpacing(0);
    horizontalLayout_2->setMargin(0);
    horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
    buttonSupprimerCompte = new QToolButton(groupBoxGestionComptes);
    buttonSupprimerCompte->setObjectName(QString::fromUtf8("buttonSupprimerCompte"));
    sizePolicy1.setHeightForWidth(buttonSupprimerCompte->sizePolicy().hasHeightForWidth());
    buttonSupprimerCompte->setSizePolicy(sizePolicy1);
    QIcon icon3;
    icon3.addPixmap(QPixmap(QString::fromUtf8(":/Images/hang_up.svg")), QIcon::Normal, QIcon::Off);
    buttonSupprimerCompte->setIcon(icon3);

    horizontalLayout_2->addWidget(buttonSupprimerCompte);

    buttonNouveauCompte = new QToolButton(groupBoxGestionComptes);
    buttonNouveauCompte->setObjectName(QString::fromUtf8("buttonNouveauCompte"));
    sizePolicy1.setHeightForWidth(buttonNouveauCompte->sizePolicy().hasHeightForWidth());
    buttonNouveauCompte->setSizePolicy(sizePolicy1);
    buttonNouveauCompte->setSizeIncrement(QSize(0, 0));
    QIcon icon4;
    icon4.addPixmap(QPixmap(QString::fromUtf8(":/Images/accept.svg")), QIcon::Normal, QIcon::Off);
    buttonNouveauCompte->setIcon(icon4);

    horizontalLayout_2->addWidget(buttonNouveauCompte);

    buttonSupprimerCompte->raise();
    buttonNouveauCompte->raise();

    verticalLayout_6->addWidget(groupBoxGestionComptes);


    horizontalLayout_3->addWidget(frame1_ListeComptes);

    frame2_EditComptes = new QFrame(widget1_ConfComptes);
    frame2_EditComptes->setObjectName(QString::fromUtf8("frame2_EditComptes"));
    QSizePolicy sizePolicy4(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sizePolicy4.setHorizontalStretch(0);
    sizePolicy4.setVerticalStretch(0);
    sizePolicy4.setHeightForWidth(frame2_EditComptes->sizePolicy().hasHeightForWidth());
    frame2_EditComptes->setSizePolicy(sizePolicy4);
    frame2_EditComptes->setFrameShape(QFrame::StyledPanel);
    frame2_EditComptes->setFrameShadow(QFrame::Raised);
    formLayout_2 = new QFormLayout(frame2_EditComptes);
    formLayout_2->setSpacing(4);
    formLayout_2->setMargin(4);
    formLayout_2->setObjectName(QString::fromUtf8("formLayout_2"));
    formLayout_2->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    label1_Alias = new QLabel(frame2_EditComptes);
    label1_Alias->setObjectName(QString::fromUtf8("label1_Alias"));

    formLayout_2->setWidget(0, QFormLayout::LabelRole, label1_Alias);

    edit1_Alias = new QLineEdit(frame2_EditComptes);
    edit1_Alias->setObjectName(QString::fromUtf8("edit1_Alias"));
    edit1_Alias->setMinimumSize(QSize(0, 0));

    formLayout_2->setWidget(0, QFormLayout::FieldRole, edit1_Alias);

    label2_Protocole = new QLabel(frame2_EditComptes);
    label2_Protocole->setObjectName(QString::fromUtf8("label2_Protocole"));

    formLayout_2->setWidget(1, QFormLayout::LabelRole, label2_Protocole);

    label3_Serveur = new QLabel(frame2_EditComptes);
    label3_Serveur->setObjectName(QString::fromUtf8("label3_Serveur"));

    formLayout_2->setWidget(2, QFormLayout::LabelRole, label3_Serveur);

    label4_Usager = new QLabel(frame2_EditComptes);
    label4_Usager->setObjectName(QString::fromUtf8("label4_Usager"));

    formLayout_2->setWidget(3, QFormLayout::LabelRole, label4_Usager);

    label5_Mdp = new QLabel(frame2_EditComptes);
    label5_Mdp->setObjectName(QString::fromUtf8("label5_Mdp"));

    formLayout_2->setWidget(4, QFormLayout::LabelRole, label5_Mdp);

    label6_BoiteVocale = new QLabel(frame2_EditComptes);
    label6_BoiteVocale->setObjectName(QString::fromUtf8("label6_BoiteVocale"));

    formLayout_2->setWidget(5, QFormLayout::LabelRole, label6_BoiteVocale);

    edit3_Serveur = new QLineEdit(frame2_EditComptes);
    edit3_Serveur->setObjectName(QString::fromUtf8("edit3_Serveur"));
    edit3_Serveur->setMinimumSize(QSize(0, 0));

    formLayout_2->setWidget(2, QFormLayout::FieldRole, edit3_Serveur);

    edit4_Usager = new QLineEdit(frame2_EditComptes);
    edit4_Usager->setObjectName(QString::fromUtf8("edit4_Usager"));

    formLayout_2->setWidget(3, QFormLayout::FieldRole, edit4_Usager);

    edit5_Mdp = new QLineEdit(frame2_EditComptes);
    edit5_Mdp->setObjectName(QString::fromUtf8("edit5_Mdp"));
    edit5_Mdp->setEchoMode(QLineEdit::Password);

    formLayout_2->setWidget(4, QFormLayout::FieldRole, edit5_Mdp);

    edit6_BoiteVocale = new QLineEdit(frame2_EditComptes);
    edit6_BoiteVocale->setObjectName(QString::fromUtf8("edit6_BoiteVocale"));

    formLayout_2->setWidget(5, QFormLayout::FieldRole, edit6_BoiteVocale);

    edit2_Protocole = new QComboBox(frame2_EditComptes);
    edit2_Protocole->setObjectName(QString::fromUtf8("edit2_Protocole"));

    formLayout_2->setWidget(1, QFormLayout::FieldRole, edit2_Protocole);


    horizontalLayout_3->addWidget(frame2_EditComptes);


    verticalLayout_2->addWidget(widget1_ConfComptes);

    groupBox_ConfComptesCommuns = new QGroupBox(page_Comptes);
    groupBox_ConfComptesCommuns->setObjectName(QString::fromUtf8("groupBox_ConfComptesCommuns"));
    verticalLayout_10 = new QVBoxLayout(groupBox_ConfComptesCommuns);
    verticalLayout_10->setSpacing(4);
    verticalLayout_10->setMargin(4);
    verticalLayout_10->setObjectName(QString::fromUtf8("verticalLayout_10"));
    label_ConfComptesCommus = new QLabel(groupBox_ConfComptesCommuns);
    label_ConfComptesCommus->setObjectName(QString::fromUtf8("label_ConfComptesCommus"));

    verticalLayout_10->addWidget(label_ConfComptesCommus);

    formLayoutConfComptesCommus = new QFormLayout();
    formLayoutConfComptesCommus->setSpacing(4);
    formLayoutConfComptesCommus->setObjectName(QString::fromUtf8("formLayoutConfComptesCommus"));
    formLayoutConfComptesCommus->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    checkBoxStun = new QCheckBox(groupBox_ConfComptesCommuns);
    checkBoxStun->setObjectName(QString::fromUtf8("checkBoxStun"));

    formLayoutConfComptesCommus->setWidget(0, QFormLayout::LabelRole, checkBoxStun);

    lineEdit_Stun = new QLineEdit(groupBox_ConfComptesCommuns);
    lineEdit_Stun->setObjectName(QString::fromUtf8("lineEdit_Stun"));
    lineEdit_Stun->setEnabled(false);

    formLayoutConfComptesCommus->setWidget(0, QFormLayout::FieldRole, lineEdit_Stun);


    verticalLayout_10->addLayout(formLayoutConfComptesCommus);


    verticalLayout_2->addWidget(groupBox_ConfComptesCommuns);

    stackedWidgetOptions->addWidget(page_Comptes);
    page_Audio = new QWidget();
    page_Audio->setObjectName(QString::fromUtf8("page_Audio"));
    verticalLayout_5 = new QVBoxLayout(page_Audio);
    verticalLayout_5->setSpacing(4);
    verticalLayout_5->setMargin(4);
    verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
    label_ConfAudio = new QLabel(page_Audio);
    label_ConfAudio->setObjectName(QString::fromUtf8("label_ConfAudio"));
    sizePolicy1.setHeightForWidth(label_ConfAudio->sizePolicy().hasHeightForWidth());
    label_ConfAudio->setSizePolicy(sizePolicy1);

    verticalLayout_5->addWidget(label_ConfAudio);

    line_ConfAudio = new QFrame(page_Audio);
    line_ConfAudio->setObjectName(QString::fromUtf8("line_ConfAudio"));
    line_ConfAudio->setFrameShape(QFrame::HLine);
    line_ConfAudio->setFrameShadow(QFrame::Sunken);

    verticalLayout_5->addWidget(line_ConfAudio);

    groupBox1_Audio = new QGroupBox(page_Audio);
    groupBox1_Audio->setObjectName(QString::fromUtf8("groupBox1_Audio"));
    formLayout_3 = new QFormLayout(groupBox1_Audio);
    formLayout_3->setSpacing(4);
    formLayout_3->setMargin(4);
    formLayout_3->setObjectName(QString::fromUtf8("formLayout_3"));
    label_Interface = new QLabel(groupBox1_Audio);
    label_Interface->setObjectName(QString::fromUtf8("label_Interface"));

    formLayout_3->setWidget(0, QFormLayout::LabelRole, label_Interface);

    comboBox_Interface = new QComboBox(groupBox1_Audio);
    comboBox_Interface->setObjectName(QString::fromUtf8("comboBox_Interface"));
    comboBox_Interface->setMinimumSize(QSize(100, 0));

    formLayout_3->setWidget(0, QFormLayout::FieldRole, comboBox_Interface);

    checkBox_Sonneries = new QCheckBox(groupBox1_Audio);
    checkBox_Sonneries->setObjectName(QString::fromUtf8("checkBox_Sonneries"));

    formLayout_3->setWidget(1, QFormLayout::LabelRole, checkBox_Sonneries);


    verticalLayout_5->addWidget(groupBox1_Audio);

    groupBox2_Codecs = new QGroupBox(page_Audio);
    groupBox2_Codecs->setObjectName(QString::fromUtf8("groupBox2_Codecs"));
    gridLayout = new QGridLayout(groupBox2_Codecs);
    gridLayout->setSpacing(4);
    gridLayout->setMargin(4);
    gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
    tableViewCodecs = new QTableView(groupBox2_Codecs);
    tableViewCodecs->setObjectName(QString::fromUtf8("tableViewCodecs"));
    tableViewCodecs->setDragEnabled(true);
    tableViewCodecs->setSortingEnabled(true);

    gridLayout->addWidget(tableViewCodecs, 0, 0, 1, 1);

    verticalLayout_OrdreCodecs = new QVBoxLayout();
    verticalLayout_OrdreCodecs->setSpacing(4);
    verticalLayout_OrdreCodecs->setObjectName(QString::fromUtf8("verticalLayout_OrdreCodecs"));
    verticalLayout_OrdreCodecs->setContentsMargins(0, -1, 0, -1);
    toolButton_MonterCodec = new QToolButton(groupBox2_Codecs);
    toolButton_MonterCodec->setObjectName(QString::fromUtf8("toolButton_MonterCodec"));

    verticalLayout_OrdreCodecs->addWidget(toolButton_MonterCodec);

    toolButton_DescendreCodec = new QToolButton(groupBox2_Codecs);
    toolButton_DescendreCodec->setObjectName(QString::fromUtf8("toolButton_DescendreCodec"));

    verticalLayout_OrdreCodecs->addWidget(toolButton_DescendreCodec);


    gridLayout->addLayout(verticalLayout_OrdreCodecs, 0, 2, 1, 1);


    verticalLayout_5->addWidget(groupBox2_Codecs);

    stackedWidget_ParametresSpecif = new QStackedWidget(page_Audio);
    stackedWidget_ParametresSpecif->setObjectName(QString::fromUtf8("stackedWidget_ParametresSpecif"));
    sizePolicy1.setHeightForWidth(stackedWidget_ParametresSpecif->sizePolicy().hasHeightForWidth());
    stackedWidget_ParametresSpecif->setSizePolicy(sizePolicy1);
    page1_Alsa = new QWidget();
    page1_Alsa->setObjectName(QString::fromUtf8("page1_Alsa"));
    verticalLayout_20 = new QVBoxLayout(page1_Alsa);
    verticalLayout_20->setSpacing(4);
    verticalLayout_20->setMargin(0);
    verticalLayout_20->setObjectName(QString::fromUtf8("verticalLayout_20"));
    groupBox_Alsa = new QGroupBox(page1_Alsa);
    groupBox_Alsa->setObjectName(QString::fromUtf8("groupBox_Alsa"));
    formLayout_4 = new QFormLayout(groupBox_Alsa);
    formLayout_4->setSpacing(4);
    formLayout_4->setMargin(4);
    formLayout_4->setObjectName(QString::fromUtf8("formLayout_4"));
    comboBox2_Entree = new QComboBox(groupBox_Alsa);
    comboBox2_Entree->setObjectName(QString::fromUtf8("comboBox2_Entree"));
    comboBox2_Entree->setMinimumSize(QSize(100, 0));

    formLayout_4->setWidget(2, QFormLayout::FieldRole, comboBox2_Entree);

    label2_Entree = new QLabel(groupBox_Alsa);
    label2_Entree->setObjectName(QString::fromUtf8("label2_Entree"));

    formLayout_4->setWidget(2, QFormLayout::LabelRole, label2_Entree);

    label3_Sortie = new QLabel(groupBox_Alsa);
    label3_Sortie->setObjectName(QString::fromUtf8("label3_Sortie"));

    formLayout_4->setWidget(3, QFormLayout::LabelRole, label3_Sortie);

    comboBox3_Sortie = new QComboBox(groupBox_Alsa);
    comboBox3_Sortie->setObjectName(QString::fromUtf8("comboBox3_Sortie"));
    comboBox3_Sortie->setMinimumSize(QSize(100, 0));

    formLayout_4->setWidget(3, QFormLayout::FieldRole, comboBox3_Sortie);

    label1_GreffonAlsa = new QLabel(groupBox_Alsa);
    label1_GreffonAlsa->setObjectName(QString::fromUtf8("label1_GreffonAlsa"));

    formLayout_4->setWidget(0, QFormLayout::LabelRole, label1_GreffonAlsa);

    comboBox1_GreffonAlsa = new QComboBox(groupBox_Alsa);
    comboBox1_GreffonAlsa->setObjectName(QString::fromUtf8("comboBox1_GreffonAlsa"));
    comboBox1_GreffonAlsa->setMinimumSize(QSize(100, 0));

    formLayout_4->setWidget(0, QFormLayout::FieldRole, comboBox1_GreffonAlsa);


    verticalLayout_20->addWidget(groupBox_Alsa);

    stackedWidget_ParametresSpecif->addWidget(page1_Alsa);
    page2_PulseAudio = new QWidget();
    page2_PulseAudio->setObjectName(QString::fromUtf8("page2_PulseAudio"));
    verticalLayout_7 = new QVBoxLayout(page2_PulseAudio);
    verticalLayout_7->setSpacing(4);
    verticalLayout_7->setMargin(4);
    verticalLayout_7->setObjectName(QString::fromUtf8("verticalLayout_7"));
    groupBox_PulseAudio = new QGroupBox(page2_PulseAudio);
    groupBox_PulseAudio->setObjectName(QString::fromUtf8("groupBox_PulseAudio"));
    QSizePolicy sizePolicy5(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sizePolicy5.setHorizontalStretch(0);
    sizePolicy5.setVerticalStretch(0);
    sizePolicy5.setHeightForWidth(groupBox_PulseAudio->sizePolicy().hasHeightForWidth());
    groupBox_PulseAudio->setSizePolicy(sizePolicy5);
    formLayout_11 = new QFormLayout(groupBox_PulseAudio);
    formLayout_11->setSpacing(4);
    formLayout_11->setMargin(4);
    formLayout_11->setObjectName(QString::fromUtf8("formLayout_11"));
    checkBox_ModifVolumeApps = new QCheckBox(groupBox_PulseAudio);
    checkBox_ModifVolumeApps->setObjectName(QString::fromUtf8("checkBox_ModifVolumeApps"));

    formLayout_11->setWidget(0, QFormLayout::LabelRole, checkBox_ModifVolumeApps);

    pushButton = new QPushButton(groupBox_PulseAudio);
    pushButton->setObjectName(QString::fromUtf8("pushButton"));
    pushButton->setDefault(false);

    formLayout_11->setWidget(1, QFormLayout::LabelRole, pushButton);


    verticalLayout_7->addWidget(groupBox_PulseAudio);

    stackedWidget_ParametresSpecif->addWidget(page2_PulseAudio);

    verticalLayout_5->addWidget(stackedWidget_ParametresSpecif);

    stackedWidgetOptions->addWidget(page_Audio);

    horizontalLayoutDialog->addWidget(stackedWidgetOptions);


    verticalLayout->addLayout(horizontalLayoutDialog);

    lineDialog = new QFrame(ConfigurationDialog);
    lineDialog->setObjectName(QString::fromUtf8("lineDialog"));
    lineDialog->setFrameShape(QFrame::HLine);
    lineDialog->setFrameShadow(QFrame::Sunken);

    verticalLayout->addWidget(lineDialog);

    buttonBoxDialog = new QDialogButtonBox(ConfigurationDialog);
    buttonBoxDialog->setObjectName(QString::fromUtf8("buttonBoxDialog"));
    buttonBoxDialog->setLayoutDirection(Qt::LeftToRight);
    buttonBoxDialog->setOrientation(Qt::Horizontal);
    buttonBoxDialog->setStandardButtons(QDialogButtonBox::Apply|QDialogButtonBox::Cancel|QDialogButtonBox::Ok|QDialogButtonBox::RestoreDefaults);
    buttonBoxDialog->setCenterButtons(false);

    verticalLayout->addWidget(buttonBoxDialog);


#ifndef QT_NO_SHORTCUT
    label_Capacite->setBuddy(horizontalSlider);
    label1_Alias->setBuddy(edit1_Alias);
    label2_Protocole->setBuddy(edit2_Protocole);
    label3_Serveur->setBuddy(edit3_Serveur);
    label4_Usager->setBuddy(edit4_Usager);
    label5_Mdp->setBuddy(edit5_Mdp);
    label6_BoiteVocale->setBuddy(edit6_BoiteVocale);
    label_Interface->setBuddy(comboBox_Interface);
    label2_Entree->setBuddy(comboBox2_Entree);
    label3_Sortie->setBuddy(comboBox3_Sortie);
    label1_GreffonAlsa->setBuddy(comboBox1_GreffonAlsa);
#endif // QT_NO_SHORTCUT


    retranslateUi(ConfigurationDialog);
    QObject::connect(buttonBoxDialog, SIGNAL(accepted()), ConfigurationDialog, SLOT(accept()));
    QObject::connect(buttonBoxDialog, SIGNAL(rejected()), ConfigurationDialog, SLOT(reject()));
    QObject::connect(listOptions, SIGNAL(currentRowChanged(int)), stackedWidgetOptions, SLOT(setCurrentIndex(int)));
    QObject::connect(checkBoxStun, SIGNAL(toggled(bool)), lineEdit_Stun, SLOT(setEnabled(bool)));
    QObject::connect(comboBox_Interface, SIGNAL(currentIndexChanged(int)), stackedWidget_ParametresSpecif, SLOT(setCurrentIndex(int)));
    QObject::connect(horizontalSlider, SIGNAL(valueChanged(int)), spinBox_CapaciteHist, SLOT(setValue(int)));
    QObject::connect(spinBox_CapaciteHist, SIGNAL(valueChanged(int)), horizontalSlider, SLOT(setValue(int)));

    stackedWidgetOptions->setCurrentIndex(2);
    stackedWidget_ParametresSpecif->setCurrentIndex(1);


    QMetaObject::connectSlotsByName(ConfigurationDialog);
    } // setupUi

    void retranslateUi(QDialog *ConfigurationDialog)
    {
    ConfigurationDialog->setWindowTitle(QApplication::translate("ConfigurationDialog", "Dialog", 0, QApplication::UnicodeUTF8));

    const bool __sortingEnabled = listOptions->isSortingEnabled();
    listOptions->setSortingEnabled(false);
    listOptions->item(0)->setText(QApplication::translate("ConfigurationDialog", "G\303\251n\303\251ral", 0, QApplication::UnicodeUTF8));
    listOptions->item(1)->setText(QApplication::translate("ConfigurationDialog", "Affichage", 0, QApplication::UnicodeUTF8));
    listOptions->item(2)->setText(QApplication::translate("ConfigurationDialog", "Comptes", 0, QApplication::UnicodeUTF8));
    listOptions->item(3)->setText(QApplication::translate("ConfigurationDialog", "Audio", 0, QApplication::UnicodeUTF8));

    listOptions->setSortingEnabled(__sortingEnabled);
    label_ConfGeneral->setText(QApplication::translate("ConfigurationDialog", "Configuration des param\303\250tres g\303\251n\303\251raux", 0, QApplication::UnicodeUTF8));
    groupBox1_Historique->setTitle(QApplication::translate("ConfigurationDialog", "Historique des appels", 0, QApplication::UnicodeUTF8));
    label_Capacite->setText(QApplication::translate("ConfigurationDialog", "&Capacit\303\251", 0, QApplication::UnicodeUTF8));
    toolButtonEffacerHist->setText(QApplication::translate("ConfigurationDialog", "&Effacer l'Historique", 0, QApplication::UnicodeUTF8));
    groupBox2_Connexion->setTitle(QApplication::translate("ConfigurationDialog", "Connexion", 0, QApplication::UnicodeUTF8));
    label_PortSIP->setText(QApplication::translate("ConfigurationDialog", "Port SIP", 0, QApplication::UnicodeUTF8));
    label_ConfAffichage->setText(QApplication::translate("ConfigurationDialog", "Configuration de l'affichage", 0, QApplication::UnicodeUTF8));
    label1_Notifications->setText(QApplication::translate("ConfigurationDialog", "Activer les notifications du bureau", 0, QApplication::UnicodeUTF8));
    checkBox1_NotifAppels->setText(QApplication::translate("ConfigurationDialog", "&Appels entrants", 0, QApplication::UnicodeUTF8));
    checkBox2_NotifMessages->setText(QApplication::translate("ConfigurationDialog", "&Messages vocaux", 0, QApplication::UnicodeUTF8));
    label2_FenetrePrincipale->setText(QApplication::translate("ConfigurationDialog", "Faire appara\303\256tre la fen\303\252tre principale", 0, QApplication::UnicodeUTF8));
    checkBox1_FenDemarrage->setText(QApplication::translate("ConfigurationDialog", "Au &d\303\251marrage", 0, QApplication::UnicodeUTF8));
    checkBox2_FenAppel->setText(QApplication::translate("ConfigurationDialog", "Lors d'un appel &entrant", 0, QApplication::UnicodeUTF8));
    label_ConfComptes->setText(QApplication::translate("ConfigurationDialog", "Configuration des comptes utilisateur", 0, QApplication::UnicodeUTF8));
    groupBoxGestionComptes->setTitle(QString());
    buttonSupprimerCompte->setText(QString());
    buttonSupprimerCompte->setShortcut(QApplication::translate("ConfigurationDialog", "+", 0, QApplication::UnicodeUTF8));
    buttonNouveauCompte->setText(QApplication::translate("ConfigurationDialog", "...", 0, QApplication::UnicodeUTF8));
    label1_Alias->setText(QApplication::translate("ConfigurationDialog", "&Alias", 0, QApplication::UnicodeUTF8));
    label2_Protocole->setText(QApplication::translate("ConfigurationDialog", "&Protocole", 0, QApplication::UnicodeUTF8));
    label3_Serveur->setText(QApplication::translate("ConfigurationDialog", "&Serveur", 0, QApplication::UnicodeUTF8));
    label4_Usager->setText(QApplication::translate("ConfigurationDialog", "&Usager", 0, QApplication::UnicodeUTF8));
    label5_Mdp->setText(QApplication::translate("ConfigurationDialog", "&Mot de Passe", 0, QApplication::UnicodeUTF8));
    label6_BoiteVocale->setText(QApplication::translate("ConfigurationDialog", "&Bo\303\256te Vocale", 0, QApplication::UnicodeUTF8));
    edit2_Protocole->clear();
    edit2_Protocole->insertItems(0, QStringList()
     << QApplication::translate("ConfigurationDialog", "SIP", 0, QApplication::UnicodeUTF8)
     << QApplication::translate("ConfigurationDialog", "IAX", 0, QApplication::UnicodeUTF8)
    );
    groupBox_ConfComptesCommuns->setTitle(QString());
    label_ConfComptesCommus->setText(QApplication::translate("ConfigurationDialog", "Les param\303\250tres STUN seront appliqu\303\251s \303\240 tous les comptes", 0, QApplication::UnicodeUTF8));
    checkBoxStun->setText(QApplication::translate("ConfigurationDialog", "Activer Stun", 0, QApplication::UnicodeUTF8));
    label_ConfAudio->setText(QApplication::translate("ConfigurationDialog", "Configuration des param\303\250tres audio", 0, QApplication::UnicodeUTF8));
    groupBox1_Audio->setTitle(QString());
    label_Interface->setText(QApplication::translate("ConfigurationDialog", "&Interface audio", 0, QApplication::UnicodeUTF8));
    comboBox_Interface->clear();
    comboBox_Interface->insertItems(0, QStringList()
     << QApplication::translate("ConfigurationDialog", "ALSA", 0, QApplication::UnicodeUTF8)
     << QApplication::translate("ConfigurationDialog", "PulseAudio", 0, QApplication::UnicodeUTF8)
    );
    checkBox_Sonneries->setText(QApplication::translate("ConfigurationDialog", "&Activer les sonneries", 0, QApplication::UnicodeUTF8));
    groupBox2_Codecs->setTitle(QApplication::translate("ConfigurationDialog", "&Codecs", 0, QApplication::UnicodeUTF8));
    toolButton_MonterCodec->setText(QApplication::translate("ConfigurationDialog", "...", 0, QApplication::UnicodeUTF8));
    toolButton_DescendreCodec->setText(QApplication::translate("ConfigurationDialog", "...", 0, QApplication::UnicodeUTF8));
    groupBox_Alsa->setTitle(QApplication::translate("ConfigurationDialog", "Param\303\250tres ALSA", 0, QApplication::UnicodeUTF8));
    label2_Entree->setText(QApplication::translate("ConfigurationDialog", "&Entr\303\251e", 0, QApplication::UnicodeUTF8));
    label3_Sortie->setText(QApplication::translate("ConfigurationDialog", "&Sortie", 0, QApplication::UnicodeUTF8));
    label1_GreffonAlsa->setText(QApplication::translate("ConfigurationDialog", "&Greffon ALSA", 0, QApplication::UnicodeUTF8));
    groupBox_PulseAudio->setTitle(QApplication::translate("ConfigurationDialog", "Param\303\250tres PulseAudio", 0, QApplication::UnicodeUTF8));
    checkBox_ModifVolumeApps->setText(QApplication::translate("ConfigurationDialog", "Autoriser \303\240 &modifier le volume des autres applications", 0, QApplication::UnicodeUTF8));
    pushButton->setText(QApplication::translate("ConfigurationDialog", "PushButton", 0, QApplication::UnicodeUTF8));
    Q_UNUSED(ConfigurationDialog);
    } // retranslateUi

};

namespace Ui {
    class ConfigurationDialog: public Ui_ConfigurationDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONFIGDIALOG_H
