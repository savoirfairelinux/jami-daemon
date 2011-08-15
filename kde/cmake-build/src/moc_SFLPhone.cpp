/****************************************************************************
** Meta object code from reading C++ file 'SFLPhone.h'
**
** Created: Tue Apr 20 16:39:53 2010
**      by: The Qt Meta Object Compiler version 62 (Qt 4.6.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/SFLPhone.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SFLPhone.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.6.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_SFLPhone[] = {

 // content:
       4,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      18,   10,    9,    9, 0x08,
      60,   10,    9,    9, 0x08,
     115,  100,    9,    9, 0x08,
     174,  162,    9,    9, 0x08,
     233,  221,    9,    9, 0x08,
     299,  280,    9,    9, 0x08,
     360,  343,    9,    9, 0x08,
     410,  402,    9,    9, 0x08,
     454,  447,    9,    9, 0x08,
     486,  481,    9,    9, 0x08,
     527,  520,    9,    9, 0x08,
     550,    9,    9,    9, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_SFLPhone[] = {
    "SFLPhone\0\0message\0"
    "on_view_statusMessageChangeAsked(QString)\0"
    "on_view_windowTitleChangeAsked(QString)\0"
    "enabledActions\0"
    "on_view_enabledActionsChangeAsked(const bool*)\0"
    "actionIcons\0on_view_actionIconsChangeAsked(const QString*)\0"
    "actionTexts\0on_view_actionTextsChangeAsked(const QString*)\0"
    "transferCheckState\0"
    "on_view_transferCheckStateChangeAsked(bool)\0"
    "recordCheckState\0"
    "on_view_recordCheckStateChangeAsked(bool)\0"
    "enabled\0on_view_addressBookEnableAsked(bool)\0"
    "screen\0on_view_screenChanged(int)\0"
    "call\0on_view_incomingCall(const Call*)\0"
    "action\0updateScreen(QAction*)\0"
    "quitButton()\0"
};

const QMetaObject SFLPhone::staticMetaObject = {
    { &KXmlGuiWindow::staticMetaObject, qt_meta_stringdata_SFLPhone,
      qt_meta_data_SFLPhone, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &SFLPhone::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *SFLPhone::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *SFLPhone::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_SFLPhone))
        return static_cast<void*>(const_cast< SFLPhone*>(this));
    return KXmlGuiWindow::qt_metacast(_clname);
}

int SFLPhone::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = KXmlGuiWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: on_view_statusMessageChangeAsked((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: on_view_windowTitleChangeAsked((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: on_view_enabledActionsChangeAsked((*reinterpret_cast< const bool*(*)>(_a[1]))); break;
        case 3: on_view_actionIconsChangeAsked((*reinterpret_cast< const QString*(*)>(_a[1]))); break;
        case 4: on_view_actionTextsChangeAsked((*reinterpret_cast< const QString*(*)>(_a[1]))); break;
        case 5: on_view_transferCheckStateChangeAsked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 6: on_view_recordCheckStateChangeAsked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 7: on_view_addressBookEnableAsked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 8: on_view_screenChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 9: on_view_incomingCall((*reinterpret_cast< const Call*(*)>(_a[1]))); break;
        case 10: updateScreen((*reinterpret_cast< QAction*(*)>(_a[1]))); break;
        case 11: quitButton(); break;
        default: ;
        }
        _id -= 12;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
