/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Jean-Philippe Barrette-LaPierre
              <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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


#ifndef __CONFIGURATION_MANAGER_IMPL_HPP__
#define __CONFIGURATION_MANAGER_IMPL_HPP__


#include <list>
#include <map>
#include <qobject.h>
#include <vector>

struct AudioDevice
{
public:
  QString index;
  QString hostApiName;
  QString deviceName;
};

struct Ringtone
{
public:
  QString index;
  QString filename;
};

struct Codec
{
public:
  QString index;
  QString codecName;
};

/**
 * This is the representation of a configuration
 * entry.
 */
struct ConfigEntry
{
public:
  ConfigEntry(){}

  ConfigEntry(QString s,
	      QString n,
	      QString t,
	      QString d,
	      QString v) 
  {
    section = s;
    name = n;
    type = t;
    def = d;
    value = v;
  }

  QString section;
  QString name;
  QString type;
  QString def;
  QString value;
};


class Session;

class ConfigurationManagerImpl : public QObject
{
  Q_OBJECT

signals:
  void audioDevicesUpdated();
  void audioDevicesInUpdated();
  void audioDevicesOutUpdated();
  void ringtonesUpdated();
  void codecsUpdated();
  void updated();
  void saved();

public:
  ConfigurationManagerImpl();
  ~ConfigurationManagerImpl();

  /**
   * will set the session to use.
   */
  void setSession(const Session &session);

  /**
   * This function will set the current speaker volume 
   * to the given percentage. If it's greater than 100
   * it will be set to 100.
   */
  void setCurrentSpeakerVolume(unsigned int percentage);

  /**
   * This function will set the current microphone volume 
   * to the given percentage. If it's greater than 100
   * it will be set to 100.
   */
  void setCurrentMicrophoneVolume(unsigned int percentage);


  void set(const QString &section, 
	   const QString &name,
	   const QString &value);
  
  QString get(const QString &section, 
	      const QString &name);
	   
  void clearAudioDevices()
  {mAudioDevices.clear();}
  void clearAudioDevicesIn()
  {mAudioDevices.clear();}
  void clearAudioDevicesOut()
  {mAudioDevices.clear();}
  
  std::list< AudioDevice > getAudioDevices()
  {return mAudioDevices;}
  std::list< AudioDevice > getAudioDevicesIn()
  {return mAudioDevicesIn;}
  std::list< AudioDevice > getAudioDevicesOut()
  {return mAudioDevicesOut;}
  
  std::list< Ringtone > getRingtones()
  {return mRingtones;}

  std::list< Codec > getCodecs()
  {return mCodecs;}

  QStringList getSkins();
  //QString getCurrentSkin(){return QString("metal");}
  
  void complete()
  {emit updated();}

  void save();
  void save(const QString& section, const QString& name);

  void finishSave();

public slots:
  void add(const ConfigEntry &entry);

  void addAudioDevice(QString index, QString hostApiName, QString deviceName);
  void add(const AudioDevice &entry);

  void addAudioDeviceIn(QString index, QString hostApiName, QString deviceName);
  void addIn(const AudioDevice &entry);

  void addAudioDeviceOut(QString index, QString hostApiName, QString deviceName);
  void addOut(const AudioDevice &entry);

  void addRingtone(QString index, QString filename);
  void add(const Ringtone &entry);

  void addCodec(QString index, QString codecName);
  void add(const Codec &entry);


private:
  typedef std::map< QString, ConfigEntry > VariableMap;
  typedef std::map< QString, VariableMap > SectionMap;
  SectionMap mEntries;

  std::list< AudioDevice > mAudioDevices;
  std::list< AudioDevice > mAudioDevicesIn;
  std::list< AudioDevice > mAudioDevicesOut;
  std::list< Ringtone > mRingtones;
  std::list< Codec > mCodecs;

  Session *mSession;
};

#endif
