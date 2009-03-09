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

#ifndef sflphone_kdeVIEW_H
#define sflphone_kdeVIEW_H

#include <QtGui/QWidget>

#include "ui_sflphone_kdeview_base.h"

class QPainter;
class KUrl;

/**
 * This is the main view class for sflphone_kde.  Most of the non-menu,
 * non-toolbar, and non-statusbar (e.g., non frame) GUI code should go
 * here.
 *
 * @short Main view
 * @author Jérémy Quentin <jeremy.quentin@gmail.com>
 * @version 0.1
 */

class sflphone_kdeView : public QWidget, public Ui::sflphone_kdeview_base
{
    Q_OBJECT
public:
    /**
     * Default constructor
     */
    sflphone_kdeView(QWidget *parent);

    /**
     * Destructor
     */
    virtual ~sflphone_kdeView();

private:
    Ui::sflphone_kdeview_base ui_sflphone_kdeview_base;

signals:
    /**
     * Use this signal to change the content of the statusbar
     */
    void signalChangeStatusbar(const QString& text);

    /**
     * Use this signal to change the content of the caption
     */
    void signalChangeCaption(const QString& text);

private slots:
    void switchColors();
    void settingsChanged();
};

#endif // sflphone_kdeVIEW_H
