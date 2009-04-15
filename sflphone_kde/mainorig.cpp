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


#include "sflphone_kde.h"
#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <KDE/KLocale>

static const char description[] =
    I18N_NOOP("A KDE 4 Application");

static const char version[] = "0.1";

int main(int argc, char **argv)
{
    KAboutData about("sflphone_kde", 0, ki18n("sflphone_kde"), version, ki18n(description),
	                 KAboutData::License_GPL, ki18n("(C) 2009 Jérémy Quentin"), KLocalizedString(), 0, "jeremy.quentin@gmail.com");
    about.addAuthor( ki18n("Jérémy Quentin"), KLocalizedString(), "jeremy.quentin@gmail.com" );
    KCmdLineArgs::init(argc, argv, &about);

    KCmdLineOptions options;
    options.add("+[URL]", ki18n( "Document to open" ));
    KCmdLineArgs::addCmdLineOptions(options);
    KApplication app;

    sflphone_kde *widget = new sflphone_kde;

    // see if we are starting with session management
    if (app.isSessionRestored())
    {
        RESTORE(sflphone_kde);
    }
    else
    {
        // no session.. just start up normally
        KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
        if (args->count() == 0)
        {
            //sflphone_kde *widget = new sflphone_kde;
            widget->show();
        }
        else
        {
            int i = 0;
            for (; i < args->count(); i++)
            {
                //sflphone_kde *widget = new sflphone_kde;
                widget->show();
            }
        }
        args->clear();
    }

    return app.exec();
}
