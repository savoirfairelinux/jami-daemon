/****************************************************************************
** Meta object code from reading C++ file 'dlgaudio.h'
**
** Created: Tue Apr 20 14:19:37 2010
**      by: The Qt Meta Object Compiler version 62 (Qt 4.6.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/conf/dlgaudio.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dlgaudio.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.6.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DlgAudio[] = {

 // content:
       4,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
      10,    9,    9,    9, 0x05,

 // slots: signature, parameters, type, tag, flags
      26,    9,    9,    9, 0x0a,
      42,    9,    9,    9, 0x0a,
      64,    9,   59,    9, 0x0a,
      77,    9,    9,    9, 0x0a,

       0        // eod
};

static const char qt_meta_stringdata_DlgAudio[] = {
    "DlgAudio\0\0updateButtons()\0updateWidgets()\0"
    "updateSettings()\0bool\0hasChanged()\0"
    "loadAlsaSettings()\0"
};

const QMetaObject DlgAudio::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DlgAudio,
      qt_meta_data_DlgAudio, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &DlgAudio::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *DlgAudio::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *DlgAudio::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DlgAudio))
        return static_cast<void*>(const_cast< DlgAudio*>(this));
    if (!strcmp(_clname, "Ui_DlgAudioBase"))
        return static_cast< Ui_DlgAudioBase*>(const_cast< DlgAudio*>(this));
    return QWidget::qt_metacast(_clname);
}

int DlgAudio::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: updateButtons(); break;
        case 1: updateWidgets(); break;
        case 2: updateSettings(); break;
        case 3: { bool _r = hasChanged();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 4: loadAlsaSettings(); break;
        default: ;
        }
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void DlgAudio::updateButtons()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}
QT_END_MOC_NAMESPACE
