/****************************************************************************
** Meta object code from reading C++ file 'ConfigurationDialog.h'
**
** Created: Tue Apr 20 14:19:39 2010
**      by: The Qt Meta Object Compiler version 62 (Qt 4.6.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/conf/ConfigurationDialog.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ConfigurationDialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.6.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_ConfigurationDialog[] = {

 // content:
       4,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: signature, parameters, type, tag, flags
      21,   20,   20,   20, 0x05,
      45,   20,   20,   20, 0x05,

 // slots: signature, parameters, type, tag, flags
      62,   20,   20,   20, 0x0a,
      78,   20,   20,   20, 0x0a,
      95,   20,   20,   20, 0x0a,
     116,   20,  111,   20, 0x0a,
     129,   20,   20,   20, 0x0a,
     138,   20,   20,   20, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_ConfigurationDialog[] = {
    "ConfigurationDialog\0\0clearCallHistoryAsked()\0"
    "changesApplied()\0updateWidgets()\0"
    "updateSettings()\0updateButtons()\0bool\0"
    "hasChanged()\0reload()\0applyCustomSettings()\0"
};

const QMetaObject ConfigurationDialog::staticMetaObject = {
    { &KConfigDialog::staticMetaObject, qt_meta_stringdata_ConfigurationDialog,
      qt_meta_data_ConfigurationDialog, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &ConfigurationDialog::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *ConfigurationDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *ConfigurationDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_ConfigurationDialog))
        return static_cast<void*>(const_cast< ConfigurationDialog*>(this));
    return KConfigDialog::qt_metacast(_clname);
}

int ConfigurationDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = KConfigDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: clearCallHistoryAsked(); break;
        case 1: changesApplied(); break;
        case 2: updateWidgets(); break;
        case 3: updateSettings(); break;
        case 4: updateButtons(); break;
        case 5: { bool _r = hasChanged();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 6: reload(); break;
        case 7: applyCustomSettings(); break;
        default: ;
        }
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void ConfigurationDialog::clearCallHistoryAsked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}

// SIGNAL 1
void ConfigurationDialog::changesApplied()
{
    QMetaObject::activate(this, &staticMetaObject, 1, 0);
}
QT_END_MOC_NAMESPACE
