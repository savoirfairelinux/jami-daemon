/***************************************************************************
 *   Copyright (C) 2009 by Jérémy Quentin   *
 *   jeremy.quentin@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "sflphone_kdeview.h"
#include "settings.h"

#include <klocale.h>
#include <QtGui/QLabel>

sflphone_kdeView::sflphone_kdeView(QWidget *)
{
    ui_sflphone_kdeview_base.setupUi(this);
    settingsChanged();
    setAutoFillBackground(true);
}

sflphone_kdeView::~sflphone_kdeView()
{

}

void sflphone_kdeView::switchColors()
{
    // switch the foreground/background colors of the label
    QColor color = Settings::col_background();
    Settings::setCol_background( Settings::col_foreground() );
    Settings::setCol_foreground( color );

    settingsChanged();
}

void sflphone_kdeView::settingsChanged()
{
    QPalette pal;
    pal.setColor( QPalette::Window, Settings::col_background());
    pal.setColor( QPalette::WindowText, Settings::col_foreground());
    ui_sflphone_kdeview_base.kcfg_sillyLabel->setPalette( pal );

    // i18n : internationalization
    ui_sflphone_kdeview_base.kcfg_sillyLabel->setText( i18n("This project is %1 days old",Settings::val_time()) );
    emit signalChangeStatusbar( i18n("Settings changed") );
}

#include "sflphone_kdeview.moc"
