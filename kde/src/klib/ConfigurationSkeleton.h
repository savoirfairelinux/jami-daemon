/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

#ifndef CONFIGURATIONSKELETON_H
#define CONFIGURATIONSKELETON_H

#include <QWidget>
#include "../lib/typedefs.h"

#include "src/klib/kcfg_settings.h"
//#include "CodecListModel.h"

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
class LIB_EXPORT ConfigurationSkeleton : public ConfigurationSkeletonBase
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
