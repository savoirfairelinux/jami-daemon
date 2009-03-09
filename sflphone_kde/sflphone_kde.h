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

#ifndef SFLPHONE_KDE_H
#define SFLPHONE_KDE_H


#include <kxmlguiwindow.h>

#include "ui_prefs_base.h"

class sflphone_kdeView;
class KPrinter;
class KToggleAction;
class KUrl;

/**
 * This class serves as the main window for sflphone_kde.  It handles the
 * menus, toolbars, and status bars.
 *
 * @short Main window class
 * @author Andreas Pakulat <apaku@gmx.de>
 * @version 0.1
 */
class sflphone_kde : public KXmlGuiWindow
{
    Q_OBJECT
public:
    /**
     * Default Constructor
     */
    sflphone_kde();

    /**
     * Default Destructor
     */
    virtual ~sflphone_kde();

private slots:
    void fileNew();
    void optionsPreferences();

private:
    void setupActions();

private:
    Ui::prefs_base ui_prefs_base ;
    sflphone_kdeView *m_view;

    KPrinter   *m_printer;
    KToggleAction *m_toolbarAction;
    KToggleAction *m_statusbarAction;
};

#endif // _sflphone_kde_H_
