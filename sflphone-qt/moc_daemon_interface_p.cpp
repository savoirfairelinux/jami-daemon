/****************************************************************************
** Meta object code from reading C++ file 'daemon_interface_p.h'
**
** Created: Wed Feb 18 11:29:38 2009
**      by: The Qt Meta Object Compiler version 59 (Qt 4.4.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "daemon_interface_p.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'daemon_interface_p.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 59
#error "This file was generated using the moc from 4.4.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DaemonInterface[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
      60,   10, // methods
       0,    0, // properties
       0,    0, // enums/sets

 // signals: signature, parameters, type, tag, flags
      17,   16,   16,   16, 0x05,
      40,   35,   16,   16, 0x05,
      64,   56,   16,   16, 0x05,

 // slots: signature, parameters, type, tag, flags
     116,   56,   99,   16, 0x0a,
     144,   16,   99,   16, 0x0a,
     195,  185,  157,   16, 0x0a,
     246,   16,  222,   16, 0x0a,
     263,   16,  222,   16, 0x0a,
     305,  300,  284,   16, 0x0a,
     334,   16,  222,   16, 0x0a,
     360,   16,  284,   16, 0x0a,
     378,   16,  222,   16, 0x0a,
     413,  405,  222,   16, 0x0a,
     434,   16,  222,   16, 0x0a,
     449,   16,  222,   16, 0x0a,
     499,   16,  479,   16, 0x0a,
     529,   16,  284,   16, 0x0a,
     542,   16,  222,   16, 0x0a,
     568,   16,  284,   16, 0x0a,
     584,   16,  284,   16, 0x0a,
     598,   16,  284,   16, 0x0a,
     610,   16,  222,   16, 0x0a,
     637,   16,  222,   16, 0x0a,
     661,   16,  284,   16, 0x0a,
     688,   16,  222,   16, 0x0a,
     710,   16,  479,   16, 0x0a,
     730,   16,  222,   16, 0x0a,
     748,   16,  284,   16, 0x0a,
     763,   16,  284,   16, 0x0a,
     776,   16,  479,   16, 0x0a,
     792,   16,  222,   16, 0x0a,
     812,   16,  479,   16, 0x0a,
     825,   16,  284,   16, 0x0a,
     845,   16,  284,   16, 0x0a,
     861,   16,  284,   16, 0x0a,
     881,   16,  284,   16, 0x0a,
     897,   16,  284,   16, 0x0a,
     913,   16,  284,   16, 0x0a,
     935,  925,   99,   16, 0x0a,
     958,   16,   99,   16, 0x0a,
     993,  976,   99,   16, 0x0a,
    1037, 1019,   99,   16, 0x0a,
    1085, 1080,   99,   16, 0x0a,
    1123, 1117,   99,   16, 0x0a,
    1152, 1148,   99,   16, 0x0a,
    1173, 1117,   99,   16, 0x0a,
    1199,   16,   99,   16, 0x0a,
    1224, 1212,   99,   16, 0x0a,
    1253,   16,   99,   16, 0x0a,
    1275, 1269,   99,   16, 0x0a,
    1292,   16,   99,   16, 0x0a,
    1304, 1212,   99,   16, 0x0a,
    1334,   16,   99,   16, 0x0a,
    1366, 1361,   99,   16, 0x0a,
    1393,   16,   99,   16, 0x0a,
    1413, 1408,   99,   16, 0x0a,
    1436, 1429,   99,   16, 0x0a,
    1459,   16,   99,   16, 0x0a,
    1479,   16,   99,   16, 0x0a,
    1493,   16,   99,   16, 0x0a,

       0        // eod
};

static const char qt_meta_stringdata_DaemonInterface[] = {
    "DaemonInterface\0\0accountsChanged()\0"
    "code\0errorAlert(int)\0details\0"
    "parametersChanged(MapStringString)\0"
    "QDBusReply<void>\0addAccount(MapStringString)\0"
    "enableStun()\0QDBusReply<MapStringString>\0"
    "accountID\0getAccountDetails(QString)\0"
    "QDBusReply<QStringList>\0getAccountList()\0"
    "getActiveCodecList()\0QDBusReply<int>\0"
    "name\0getAudioDeviceIndex(QString)\0"
    "getAudioInputDeviceList()\0getAudioManager()\0"
    "getAudioOutputDeviceList()\0payload\0"
    "getCodecDetails(int)\0getCodecList()\0"
    "getCurrentAudioDevicesIndex()\0"
    "QDBusReply<QString>\0getCurrentAudioOutputPlugin()\0"
    "getDialpad()\0getInputAudioPluginList()\0"
    "getMailNotify()\0getMaxCalls()\0getNotify()\0"
    "getOutputAudioPluginList()\0"
    "getPlaybackDeviceList()\0"
    "getPulseAppVolumeControl()\0"
    "getRecordDeviceList()\0getRingtoneChoice()\0"
    "getRingtoneList()\0getSearchbar()\0"
    "getSipPort()\0getStunServer()\0"
    "getToneLocaleList()\0getVersion()\0"
    "getVolumeControls()\0isIax2Enabled()\0"
    "isRingtoneEnabled()\0isStartHidden()\0"
    "isStunEnabled()\0popupMode()\0accoundID\0"
    "removeAccount(QString)\0ringtoneEnabled()\0"
    "accountID,expire\0sendRegister(QString,int)\0"
    "accountID,details\0"
    "setAccountDetails(QString,MapStringString)\0"
    "list\0setActiveCodecList(QStringList)\0"
    "index\0setAudioInputDevice(int)\0api\0"
    "setAudioManager(int)\0setAudioOutputDevice(int)\0"
    "setDialpad()\0audioPlugin\0"
    "setInputAudioPlugin(QString)\0"
    "setMailNotify()\0calls\0setMaxCalls(int)\0"
    "setNotify()\0setOutputAudioPlugin(QString)\0"
    "setPulseAppVolumeControl()\0tone\0"
    "setRingtoneChoice(QString)\0setSearchbar()\0"
    "port\0setSipPort(int)\0server\0"
    "setStunServer(QString)\0setVolumeControls()\0"
    "startHidden()\0switchPopupMode()\0"
};

const QMetaObject DaemonInterface::staticMetaObject = {
    { &QDBusAbstractInterface::staticMetaObject, qt_meta_stringdata_DaemonInterface,
      qt_meta_data_DaemonInterface, 0 }
};

const QMetaObject *DaemonInterface::metaObject() const
{
    return &staticMetaObject;
}

void *DaemonInterface::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DaemonInterface))
        return static_cast<void*>(const_cast< DaemonInterface*>(this));
    return QDBusAbstractInterface::qt_metacast(_clname);
}

int DaemonInterface::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDBusAbstractInterface::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: accountsChanged(); break;
        case 1: errorAlert((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: parametersChanged((*reinterpret_cast< MapStringString(*)>(_a[1]))); break;
        case 3: { QDBusReply<void> _r = addAccount((*reinterpret_cast< MapStringString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 4: { QDBusReply<void> _r = enableStun();
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 5: { QDBusReply<MapStringString> _r = getAccountDetails((*reinterpret_cast< const QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<MapStringString>*>(_a[0]) = _r; }  break;
        case 6: { QDBusReply<QStringList> _r = getAccountList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 7: { QDBusReply<QStringList> _r = getActiveCodecList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 8: { QDBusReply<int> _r = getAudioDeviceIndex((*reinterpret_cast< const QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 9: { QDBusReply<QStringList> _r = getAudioInputDeviceList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 10: { QDBusReply<int> _r = getAudioManager();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 11: { QDBusReply<QStringList> _r = getAudioOutputDeviceList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 12: { QDBusReply<QStringList> _r = getCodecDetails((*reinterpret_cast< int(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 13: { QDBusReply<QStringList> _r = getCodecList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 14: { QDBusReply<QStringList> _r = getCurrentAudioDevicesIndex();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 15: { QDBusReply<QString> _r = getCurrentAudioOutputPlugin();
            if (_a[0]) *reinterpret_cast< QDBusReply<QString>*>(_a[0]) = _r; }  break;
        case 16: { QDBusReply<int> _r = getDialpad();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 17: { QDBusReply<QStringList> _r = getInputAudioPluginList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 18: { QDBusReply<int> _r = getMailNotify();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 19: { QDBusReply<int> _r = getMaxCalls();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 20: { QDBusReply<int> _r = getNotify();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 21: { QDBusReply<QStringList> _r = getOutputAudioPluginList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 22: { QDBusReply<QStringList> _r = getPlaybackDeviceList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 23: { QDBusReply<int> _r = getPulseAppVolumeControl();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 24: { QDBusReply<QStringList> _r = getRecordDeviceList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 25: { QDBusReply<QString> _r = getRingtoneChoice();
            if (_a[0]) *reinterpret_cast< QDBusReply<QString>*>(_a[0]) = _r; }  break;
        case 26: { QDBusReply<QStringList> _r = getRingtoneList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 27: { QDBusReply<int> _r = getSearchbar();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 28: { QDBusReply<int> _r = getSipPort();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 29: { QDBusReply<QString> _r = getStunServer();
            if (_a[0]) *reinterpret_cast< QDBusReply<QString>*>(_a[0]) = _r; }  break;
        case 30: { QDBusReply<QStringList> _r = getToneLocaleList();
            if (_a[0]) *reinterpret_cast< QDBusReply<QStringList>*>(_a[0]) = _r; }  break;
        case 31: { QDBusReply<QString> _r = getVersion();
            if (_a[0]) *reinterpret_cast< QDBusReply<QString>*>(_a[0]) = _r; }  break;
        case 32: { QDBusReply<int> _r = getVolumeControls();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 33: { QDBusReply<int> _r = isIax2Enabled();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 34: { QDBusReply<int> _r = isRingtoneEnabled();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 35: { QDBusReply<int> _r = isStartHidden();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 36: { QDBusReply<int> _r = isStunEnabled();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 37: { QDBusReply<int> _r = popupMode();
            if (_a[0]) *reinterpret_cast< QDBusReply<int>*>(_a[0]) = _r; }  break;
        case 38: { QDBusReply<void> _r = removeAccount((*reinterpret_cast< const QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 39: { QDBusReply<void> _r = ringtoneEnabled();
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 40: { QDBusReply<void> _r = sendRegister((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 41: { QDBusReply<void> _r = setAccountDetails((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< MapStringString(*)>(_a[2])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 42: { QDBusReply<void> _r = setActiveCodecList((*reinterpret_cast< const QStringList(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 43: { QDBusReply<void> _r = setAudioInputDevice((*reinterpret_cast< int(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 44: { QDBusReply<void> _r = setAudioManager((*reinterpret_cast< int(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 45: { QDBusReply<void> _r = setAudioOutputDevice((*reinterpret_cast< int(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 46: { QDBusReply<void> _r = setDialpad();
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 47: { QDBusReply<void> _r = setInputAudioPlugin((*reinterpret_cast< const QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 48: { QDBusReply<void> _r = setMailNotify();
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 49: { QDBusReply<void> _r = setMaxCalls((*reinterpret_cast< int(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 50: { QDBusReply<void> _r = setNotify();
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 51: { QDBusReply<void> _r = setOutputAudioPlugin((*reinterpret_cast< const QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 52: { QDBusReply<void> _r = setPulseAppVolumeControl();
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 53: { QDBusReply<void> _r = setRingtoneChoice((*reinterpret_cast< const QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 54: { QDBusReply<void> _r = setSearchbar();
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 55: { QDBusReply<void> _r = setSipPort((*reinterpret_cast< int(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 56: { QDBusReply<void> _r = setStunServer((*reinterpret_cast< const QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 57: { QDBusReply<void> _r = setVolumeControls();
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 58: { QDBusReply<void> _r = startHidden();
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        case 59: { QDBusReply<void> _r = switchPopupMode();
            if (_a[0]) *reinterpret_cast< QDBusReply<void>*>(_a[0]) = _r; }  break;
        }
        _id -= 60;
    }
    return _id;
}

// SIGNAL 0
void DaemonInterface::accountsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}

// SIGNAL 1
void DaemonInterface::errorAlert(int _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void DaemonInterface::parametersChanged(MapStringString _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_END_MOC_NAMESPACE
