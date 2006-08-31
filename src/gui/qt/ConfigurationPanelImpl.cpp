/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *                                                                              
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                              
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <qbutton.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qsizepolicy.h>

#include "ConfigurationPanelImpl.hpp"

ConfigurationPanelImpl::ConfigurationPanelImpl(QWidget *parent)
  : QDialog(parent)
{
  mLayout = new QVBoxLayout(this);

}

void
ConfigurationPanelImpl::add(const ConfigEntry &entry)
{
  mEntries[entry.section].push_back(entry);
}

void
ConfigurationPanelImpl::generate()
{
  std::map< QString, std::list< ConfigEntry > >::iterator pos = mEntries.begin();
  while(pos != mEntries.end()) {
    QVBoxLayout *l = new QVBoxLayout(this);
    
    std::list< ConfigEntry > entries = pos->second;
    std::list< ConfigEntry >::iterator entrypos = entries.begin();
    while(entrypos != entries.end()) {
      QHBox *hbox = new QHBox(this);
      mLayout->addWidget(hbox);

      QLabel *label = new QLabel(hbox);
      label->setText((*entrypos).name);
      QLineEdit *edit = new QLineEdit(hbox);
      edit->setText((*entrypos).value);

      entrypos++;
    }

    pos++;
  }

  QButton *ok = new QButton(this);
  ok->setText(QObject::tr("Ok"));
  mLayout->addWidget(ok);

  show();
}


