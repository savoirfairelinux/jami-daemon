#include <kdialog.h>
#include <klocale.h>

/********************************************************************************
** Form generated from reading UI file 'dlgaudiobase.ui'
**
** Created: Tue Apr 20 14:19:42 2010
**      by: Qt User Interface Compiler version 4.6.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DLGAUDIOBASE_H
#define UI_DLGAUDIOBASE_H

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
#include <QtGui/QStackedWidget>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>
#include "kcombobox.h"
#include "kurlrequester.h"

QT_BEGIN_NAMESPACE

class Ui_DlgAudioBase
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox1_audio;
    QFormLayout *formLayout_3;
    QLabel *label_interface;
    KComboBox *kcfg_interface;
    QCheckBox *kcfg_enableRingtones;
    KUrlRequester *KUrlRequester_ringtone;
    QStackedWidget *stackedWidget_interfaceSpecificSettings;
    QWidget *page1_alsa;
    QVBoxLayout *verticalLayout_20;
    QGroupBox *groupBox_alsa;
    QFormLayout *formLayout_4;
    QLabel *label1_alsaPugin;
    KComboBox *box_alsaPlugin;
    QLabel *label2_in;
    KComboBox *kcfg_alsaInputDevice;
    QLabel *label3_out;
    KComboBox *kcfg_alsaOutputDevice;
    QWidget *page2_pulseAudio;
    QVBoxLayout *verticalLayout_7;
    QGroupBox *groupBox_pulseAudio;
    QFormLayout *formLayout_11;
    QCheckBox *kcfg_pulseAudioVolumeAlter;
    QGroupBox *groupBox1_recordGeneral;
    QHBoxLayout *horizontalLayout;
    QLabel *label_destinationFolderd;
    KUrlRequester *KUrlRequester_destinationFolder;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *DlgAudioBase)
    {
        if (DlgAudioBase->objectName().isEmpty())
            DlgAudioBase->setObjectName(QString::fromUtf8("DlgAudioBase"));
        DlgAudioBase->resize(467, 437);
        DlgAudioBase->setWindowTitle(QString::fromUtf8("Form"));
        verticalLayout = new QVBoxLayout(DlgAudioBase);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        groupBox1_audio = new QGroupBox(DlgAudioBase);
        groupBox1_audio->setObjectName(QString::fromUtf8("groupBox1_audio"));
        groupBox1_audio->setMouseTracking(false);
        formLayout_3 = new QFormLayout(groupBox1_audio);
        formLayout_3->setObjectName(QString::fromUtf8("formLayout_3"));
        formLayout_3->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        label_interface = new QLabel(groupBox1_audio);
        label_interface->setObjectName(QString::fromUtf8("label_interface"));

        formLayout_3->setWidget(0, QFormLayout::LabelRole, label_interface);

        kcfg_interface = new KComboBox(groupBox1_audio);
        kcfg_interface->setObjectName(QString::fromUtf8("kcfg_interface"));

        formLayout_3->setWidget(0, QFormLayout::FieldRole, kcfg_interface);

        kcfg_enableRingtones = new QCheckBox(groupBox1_audio);
        kcfg_enableRingtones->setObjectName(QString::fromUtf8("kcfg_enableRingtones"));

        formLayout_3->setWidget(2, QFormLayout::LabelRole, kcfg_enableRingtones);

        KUrlRequester_ringtone = new KUrlRequester(groupBox1_audio);
        KUrlRequester_ringtone->setObjectName(QString::fromUtf8("KUrlRequester_ringtone"));
        KUrlRequester_ringtone->setFilter(QString::fromUtf8("*.ul *.au *.wav"));

        formLayout_3->setWidget(2, QFormLayout::FieldRole, KUrlRequester_ringtone);


        verticalLayout->addWidget(groupBox1_audio);

        stackedWidget_interfaceSpecificSettings = new QStackedWidget(DlgAudioBase);
        stackedWidget_interfaceSpecificSettings->setObjectName(QString::fromUtf8("stackedWidget_interfaceSpecificSettings"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(stackedWidget_interfaceSpecificSettings->sizePolicy().hasHeightForWidth());
        stackedWidget_interfaceSpecificSettings->setSizePolicy(sizePolicy);
        page1_alsa = new QWidget();
        page1_alsa->setObjectName(QString::fromUtf8("page1_alsa"));
        verticalLayout_20 = new QVBoxLayout(page1_alsa);
        verticalLayout_20->setContentsMargins(0, 0, 0, 0);
        verticalLayout_20->setObjectName(QString::fromUtf8("verticalLayout_20"));
        groupBox_alsa = new QGroupBox(page1_alsa);
        groupBox_alsa->setObjectName(QString::fromUtf8("groupBox_alsa"));
        formLayout_4 = new QFormLayout(groupBox_alsa);
        formLayout_4->setObjectName(QString::fromUtf8("formLayout_4"));
        formLayout_4->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        formLayout_4->setVerticalSpacing(5);
        formLayout_4->setContentsMargins(9, -1, -1, -1);
        label1_alsaPugin = new QLabel(groupBox_alsa);
        label1_alsaPugin->setObjectName(QString::fromUtf8("label1_alsaPugin"));

        formLayout_4->setWidget(0, QFormLayout::LabelRole, label1_alsaPugin);

        box_alsaPlugin = new KComboBox(groupBox_alsa);
        box_alsaPlugin->setObjectName(QString::fromUtf8("box_alsaPlugin"));
        box_alsaPlugin->setSizeAdjustPolicy(QComboBox::AdjustToContents);

        formLayout_4->setWidget(0, QFormLayout::FieldRole, box_alsaPlugin);

        label2_in = new QLabel(groupBox_alsa);
        label2_in->setObjectName(QString::fromUtf8("label2_in"));

        formLayout_4->setWidget(2, QFormLayout::LabelRole, label2_in);

        kcfg_alsaInputDevice = new KComboBox(groupBox_alsa);
        kcfg_alsaInputDevice->setObjectName(QString::fromUtf8("kcfg_alsaInputDevice"));
        kcfg_alsaInputDevice->setSizeAdjustPolicy(QComboBox::AdjustToContents);

        formLayout_4->setWidget(2, QFormLayout::FieldRole, kcfg_alsaInputDevice);

        label3_out = new QLabel(groupBox_alsa);
        label3_out->setObjectName(QString::fromUtf8("label3_out"));

        formLayout_4->setWidget(4, QFormLayout::LabelRole, label3_out);

        kcfg_alsaOutputDevice = new KComboBox(groupBox_alsa);
        kcfg_alsaOutputDevice->setObjectName(QString::fromUtf8("kcfg_alsaOutputDevice"));
        kcfg_alsaOutputDevice->setSizeAdjustPolicy(QComboBox::AdjustToContents);

        formLayout_4->setWidget(4, QFormLayout::FieldRole, kcfg_alsaOutputDevice);


        verticalLayout_20->addWidget(groupBox_alsa);

        stackedWidget_interfaceSpecificSettings->addWidget(page1_alsa);
        page2_pulseAudio = new QWidget();
        page2_pulseAudio->setObjectName(QString::fromUtf8("page2_pulseAudio"));
        verticalLayout_7 = new QVBoxLayout(page2_pulseAudio);
        verticalLayout_7->setObjectName(QString::fromUtf8("verticalLayout_7"));
        groupBox_pulseAudio = new QGroupBox(page2_pulseAudio);
        groupBox_pulseAudio->setObjectName(QString::fromUtf8("groupBox_pulseAudio"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(groupBox_pulseAudio->sizePolicy().hasHeightForWidth());
        groupBox_pulseAudio->setSizePolicy(sizePolicy1);
        formLayout_11 = new QFormLayout(groupBox_pulseAudio);
        formLayout_11->setObjectName(QString::fromUtf8("formLayout_11"));
        formLayout_11->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        kcfg_pulseAudioVolumeAlter = new QCheckBox(groupBox_pulseAudio);
        kcfg_pulseAudioVolumeAlter->setObjectName(QString::fromUtf8("kcfg_pulseAudioVolumeAlter"));

        formLayout_11->setWidget(0, QFormLayout::LabelRole, kcfg_pulseAudioVolumeAlter);


        verticalLayout_7->addWidget(groupBox_pulseAudio);

        stackedWidget_interfaceSpecificSettings->addWidget(page2_pulseAudio);

        verticalLayout->addWidget(stackedWidget_interfaceSpecificSettings);

        groupBox1_recordGeneral = new QGroupBox(DlgAudioBase);
        groupBox1_recordGeneral->setObjectName(QString::fromUtf8("groupBox1_recordGeneral"));
        sizePolicy.setHeightForWidth(groupBox1_recordGeneral->sizePolicy().hasHeightForWidth());
        groupBox1_recordGeneral->setSizePolicy(sizePolicy);
        horizontalLayout = new QHBoxLayout(groupBox1_recordGeneral);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        label_destinationFolderd = new QLabel(groupBox1_recordGeneral);
        label_destinationFolderd->setObjectName(QString::fromUtf8("label_destinationFolderd"));

        horizontalLayout->addWidget(label_destinationFolderd);

        KUrlRequester_destinationFolder = new KUrlRequester(groupBox1_recordGeneral);
        KUrlRequester_destinationFolder->setObjectName(QString::fromUtf8("KUrlRequester_destinationFolder"));
        QSizePolicy sizePolicy2(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(KUrlRequester_destinationFolder->sizePolicy().hasHeightForWidth());
        KUrlRequester_destinationFolder->setSizePolicy(sizePolicy2);

        horizontalLayout->addWidget(KUrlRequester_destinationFolder);


        verticalLayout->addWidget(groupBox1_recordGeneral);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);

#ifndef UI_QT_NO_SHORTCUT
        label_interface->setBuddy(kcfg_interface);
        label1_alsaPugin->setBuddy(box_alsaPlugin);
        label2_in->setBuddy(kcfg_alsaInputDevice);
        label3_out->setBuddy(kcfg_alsaOutputDevice);
        label_destinationFolderd->setBuddy(KUrlRequester_destinationFolder);
#endif // QT_NO_SHORTCUT

        retranslateUi(DlgAudioBase);
        QObject::connect(kcfg_interface, SIGNAL(currentIndexChanged(int)), stackedWidget_interfaceSpecificSettings, SLOT(setCurrentIndex(int)));

        stackedWidget_interfaceSpecificSettings->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(DlgAudioBase);
    } // setupUi

    void retranslateUi(QWidget *DlgAudioBase)
    {
        groupBox1_audio->setTitle(QString());
        label_interface->setText(tr2i18n("Sound manager", 0));
        kcfg_interface->clear();
        kcfg_interface->insertItems(0, QStringList()
         << tr2i18n("ALSA", 0)
         << tr2i18n("PulseAudio", 0)
        );
        kcfg_enableRingtones->setText(tr2i18n("Enable ringtones", 0));
        groupBox_alsa->setTitle(tr2i18n("ALSA settings", 0));
        label1_alsaPugin->setText(tr2i18n("ALSA plugin", 0));
        label2_in->setText(tr2i18n("Input", 0));
        label3_out->setText(tr2i18n("Output", 0));
        groupBox_pulseAudio->setTitle(tr2i18n("PulseAudio settings", 0));
        kcfg_pulseAudioVolumeAlter->setText(tr2i18n("Mute other applications during a call", 0));
        groupBox1_recordGeneral->setTitle(tr2i18n("Recording", 0));
        label_destinationFolderd->setText(tr2i18n("Destination folder", 0));
        Q_UNUSED(DlgAudioBase);
    } // retranslateUi

};

namespace Ui {
    class DlgAudioBase: public Ui_DlgAudioBase {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DLGAUDIOBASE_H

