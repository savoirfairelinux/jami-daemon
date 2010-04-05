/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
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
#ifndef CONFIGURATIONSKELETON_H
#define CONFIGURATIONSKELETON_H

#include <QWidget>

#include "kcfg_settings.h"
//#include "CodecListModel.h"
#include "AccountListModel.h"

/**
   @author Jérémy Quentin <jeremy.quentin@gmail.com>
   This class represents the config skeleton for the config dialog.
   It inherits the KConfigSkeleton "ConfigurationSkeletonBase"generated 
   by sflphone-client-kde.kcfg which handles most of the settings.
   This class handles the codec list. 
   A few complicated settings are handled directly by the config dialog 
   and its pages (accounts, sound managers).
   This class reimplements the writeConfig and readConfig functions to ask the
   daemon instead of the normal behavior (read and write in a kconfig file).
*/
class ConfigurationSkeleton : public ConfigurationSkeletonBase
{
Q_OBJECT

private:
   static ConfigurationSkeleton * instance;
   
   //CodecListModel * codecListModel;

public:
   ConfigurationSkeleton();

   ~ConfigurationSkeleton();
    
   /**
    *   @copydoc KCoreConfigSkeleton::readConfig()
    */
   virtual void readConfig();
    
   /**
    * @copydoc KCoreConfigSkeleton::writeConfig()
    */
   virtual void writeConfig();
   
   
   static ConfigurationSkeleton * self();
   
   //QStringList activeCodecList() const;
   //void setActiveCodecList(const QStringList & v);
   
   //CodecListModel * getCodecListModel();

};

#endif
