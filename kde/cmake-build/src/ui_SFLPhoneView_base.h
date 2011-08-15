#include <kdialog.h>
#include <klocale.h>

/********************************************************************************
** Form generated from reading UI file 'SFLPhoneView_base.ui'
**
** Created: Tue Apr 20 14:19:41 2010
**      by: Qt User Interface Compiler version 4.6.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SFLPHONEVIEW_BASE_H
#define UI_SFLPHONEVIEW_BASE_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QGridLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QListWidget>
#include <QtGui/QSlider>
#include <QtGui/QStackedWidget>
#include <QtGui/QToolButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>
#include "Dialpad.h"
#include "klineedit.h"

QT_BEGIN_NAMESPACE

class Ui_SFLPhone_view
{
public:
    QVBoxLayout *verticalLayout;
    QStackedWidget *stackedWidget_screen;
    QWidget *page_callList;
    QHBoxLayout *horizontalLayout_3;
    QWidget *page_callHistory;
    QVBoxLayout *verticalLayout_2;
    QListWidget *listWidget_callHistory;
    KLineEdit *lineEdit_searchHistory;
    QWidget *page_addressBook;
    QVBoxLayout *verticalLayout_5;
    QListWidget *listWidget_addressBook;
    KLineEdit *lineEdit_addressBook;
    QLabel *label_addressBookFull;
    QWidget *widget_controls;
    QGridLayout *gridLayout;
    QSlider *slider_recVol_2;
    Dialpad *widget_dialpad;
    QSlider *slider_sndVol_2;
    QToolButton *toolButton_recVol_2;
    QToolButton *toolButton_sndVol_2;
    QToolButton *toolButton_recVol;
    QSlider *slider_recVol;
    QToolButton *toolButton_sndVol;
    QSlider *slider_sndVol;

    void setupUi(QWidget *SFLPhone_view)
    {
        if (SFLPhone_view->objectName().isEmpty())
            SFLPhone_view->setObjectName(QString::fromUtf8("SFLPhone_view"));
        SFLPhone_view->resize(337, 526);
        SFLPhone_view->setWindowTitle(QString::fromUtf8("Form"));
        verticalLayout = new QVBoxLayout(SFLPhone_view);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        stackedWidget_screen = new QStackedWidget(SFLPhone_view);
        stackedWidget_screen->setObjectName(QString::fromUtf8("stackedWidget_screen"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(stackedWidget_screen->sizePolicy().hasHeightForWidth());
        stackedWidget_screen->setSizePolicy(sizePolicy);
        page_callList = new QWidget();
        page_callList->setObjectName(QString::fromUtf8("page_callList"));
        horizontalLayout_3 = new QHBoxLayout(page_callList);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        stackedWidget_screen->addWidget(page_callList);
        page_callHistory = new QWidget();
        page_callHistory->setObjectName(QString::fromUtf8("page_callHistory"));
        verticalLayout_2 = new QVBoxLayout(page_callHistory);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        listWidget_callHistory = new QListWidget(page_callHistory);
        listWidget_callHistory->setObjectName(QString::fromUtf8("listWidget_callHistory"));

        verticalLayout_2->addWidget(listWidget_callHistory);

        lineEdit_searchHistory = new KLineEdit(page_callHistory);
        lineEdit_searchHistory->setObjectName(QString::fromUtf8("lineEdit_searchHistory"));
        lineEdit_searchHistory->setProperty("showClearButton", QVariant(true));

        verticalLayout_2->addWidget(lineEdit_searchHistory);

        stackedWidget_screen->addWidget(page_callHistory);
        page_addressBook = new QWidget();
        page_addressBook->setObjectName(QString::fromUtf8("page_addressBook"));
        verticalLayout_5 = new QVBoxLayout(page_addressBook);
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        listWidget_addressBook = new QListWidget(page_addressBook);
        listWidget_addressBook->setObjectName(QString::fromUtf8("listWidget_addressBook"));

        verticalLayout_5->addWidget(listWidget_addressBook);

        lineEdit_addressBook = new KLineEdit(page_addressBook);
        lineEdit_addressBook->setObjectName(QString::fromUtf8("lineEdit_addressBook"));
        lineEdit_addressBook->setEnabled(true);
        lineEdit_addressBook->setProperty("showClearButton", QVariant(true));

        verticalLayout_5->addWidget(lineEdit_addressBook);

        label_addressBookFull = new QLabel(page_addressBook);
        label_addressBookFull->setObjectName(QString::fromUtf8("label_addressBookFull"));
        label_addressBookFull->setWordWrap(true);

        verticalLayout_5->addWidget(label_addressBookFull);

        stackedWidget_screen->addWidget(page_addressBook);

        verticalLayout->addWidget(stackedWidget_screen);

        widget_controls = new QWidget(SFLPhone_view);
        widget_controls->setObjectName(QString::fromUtf8("widget_controls"));
        widget_controls->setEnabled(true);
        gridLayout = new QGridLayout(widget_controls);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        slider_recVol_2 = new QSlider(widget_controls);
        slider_recVol_2->setObjectName(QString::fromUtf8("slider_recVol_2"));
        QSizePolicy sizePolicy1(QSizePolicy::Fixed, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(slider_recVol_2->sizePolicy().hasHeightForWidth());
        slider_recVol_2->setSizePolicy(sizePolicy1);
        slider_recVol_2->setMinimumSize(QSize(0, 50));
        slider_recVol_2->setLayoutDirection(Qt::RightToLeft);
        slider_recVol_2->setAutoFillBackground(false);
        slider_recVol_2->setMaximum(100);
        slider_recVol_2->setOrientation(Qt::Vertical);
        slider_recVol_2->setInvertedAppearance(false);
        slider_recVol_2->setInvertedControls(false);
        slider_recVol_2->setTickPosition(QSlider::NoTicks);

        gridLayout->addWidget(slider_recVol_2, 0, 0, 1, 1);

        widget_dialpad = new Dialpad(widget_controls);
        widget_dialpad->setObjectName(QString::fromUtf8("widget_dialpad"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(widget_dialpad->sizePolicy().hasHeightForWidth());
        widget_dialpad->setSizePolicy(sizePolicy2);

        gridLayout->addWidget(widget_dialpad, 0, 1, 2, 2);

        slider_sndVol_2 = new QSlider(widget_controls);
        slider_sndVol_2->setObjectName(QString::fromUtf8("slider_sndVol_2"));
        sizePolicy1.setHeightForWidth(slider_sndVol_2->sizePolicy().hasHeightForWidth());
        slider_sndVol_2->setSizePolicy(sizePolicy1);
        slider_sndVol_2->setMinimumSize(QSize(0, 50));
        slider_sndVol_2->setLayoutDirection(Qt::LeftToRight);
        slider_sndVol_2->setAutoFillBackground(false);
        slider_sndVol_2->setMaximum(100);
        slider_sndVol_2->setOrientation(Qt::Vertical);
        slider_sndVol_2->setTickPosition(QSlider::NoTicks);

        gridLayout->addWidget(slider_sndVol_2, 0, 3, 1, 1);

        toolButton_recVol_2 = new QToolButton(widget_controls);
        toolButton_recVol_2->setObjectName(QString::fromUtf8("toolButton_recVol_2"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/Images/mic_75.svg"), QSize(), QIcon::Normal, QIcon::Off);
        toolButton_recVol_2->setIcon(icon);
        toolButton_recVol_2->setCheckable(true);

        gridLayout->addWidget(toolButton_recVol_2, 1, 0, 1, 1);

        toolButton_sndVol_2 = new QToolButton(widget_controls);
        toolButton_sndVol_2->setObjectName(QString::fromUtf8("toolButton_sndVol_2"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/Images/speaker_75.svg"), QSize(), QIcon::Normal, QIcon::Off);
        toolButton_sndVol_2->setIcon(icon1);
        toolButton_sndVol_2->setCheckable(true);
        toolButton_sndVol_2->setChecked(false);

        gridLayout->addWidget(toolButton_sndVol_2, 1, 3, 1, 1);

        toolButton_recVol = new QToolButton(widget_controls);
        toolButton_recVol->setObjectName(QString::fromUtf8("toolButton_recVol"));
        toolButton_recVol->setIcon(icon);
        toolButton_recVol->setCheckable(true);

        gridLayout->addWidget(toolButton_recVol, 2, 0, 1, 2);

        slider_recVol = new QSlider(widget_controls);
        slider_recVol->setObjectName(QString::fromUtf8("slider_recVol"));
        slider_recVol->setOrientation(Qt::Horizontal);

        gridLayout->addWidget(slider_recVol, 2, 2, 1, 2);

        toolButton_sndVol = new QToolButton(widget_controls);
        toolButton_sndVol->setObjectName(QString::fromUtf8("toolButton_sndVol"));
        toolButton_sndVol->setIcon(icon);
        toolButton_sndVol->setCheckable(true);

        gridLayout->addWidget(toolButton_sndVol, 3, 0, 1, 2);

        slider_sndVol = new QSlider(widget_controls);
        slider_sndVol->setObjectName(QString::fromUtf8("slider_sndVol"));
        slider_sndVol->setOrientation(Qt::Horizontal);

        gridLayout->addWidget(slider_sndVol, 3, 2, 1, 2);


        verticalLayout->addWidget(widget_controls);


        retranslateUi(SFLPhone_view);
        QObject::connect(toolButton_sndVol, SIGNAL(toggled(bool)), toolButton_sndVol_2, SLOT(setChecked(bool)));
        QObject::connect(toolButton_recVol, SIGNAL(toggled(bool)), toolButton_recVol_2, SLOT(setChecked(bool)));
        QObject::connect(slider_recVol, SIGNAL(valueChanged(int)), slider_recVol_2, SLOT(setValue(int)));
        QObject::connect(slider_sndVol, SIGNAL(valueChanged(int)), slider_sndVol_2, SLOT(setValue(int)));

        stackedWidget_screen->setCurrentIndex(1);


        QMetaObject::connectSlotsByName(SFLPhone_view);
    } // setupUi

    void retranslateUi(QWidget *SFLPhone_view)
    {
        lineEdit_addressBook->setText(QString());
        label_addressBookFull->setText(tr2i18n("Attention:number of results exceeds max displayed.", 0));
#ifndef UI_QT_NO_TOOLTIP
        slider_recVol_2->setToolTip(tr2i18n("Mic volume", 0));
#endif // QT_NO_TOOLTIP
#ifndef UI_QT_NO_TOOLTIP
        slider_sndVol_2->setToolTip(tr2i18n("Speakers volume", 0));
#endif // QT_NO_TOOLTIP
        toolButton_recVol_2->setText(QString());
        toolButton_sndVol_2->setText(QString());
        toolButton_recVol->setText(QString());
        toolButton_sndVol->setText(QString());
        Q_UNUSED(SFLPhone_view);
    } // retranslateUi

};

namespace Ui {
    class SFLPhone_view: public Ui_SFLPhone_view {};
} // namespace Ui

QT_END_NAMESPACE

#endif // SFLPHONEVIEW_BASE_H

