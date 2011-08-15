/****************************************************************************
** Meta object code from reading C++ file 'Dialpad.h'
**
** Created: Tue Apr 20 14:19:39 2010
**      by: The Qt Meta Object Compiler version 62 (Qt 4.6.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/Dialpad.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Dialpad.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.6.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_Dialpad[] = {

 // content:
       4,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
      14,    9,    8,    8, 0x05,

 // slots: signature, parameters, type, tag, flags
      29,    8,    8,    8, 0x08,
      55,    8,    8,    8, 0x08,
      81,    8,    8,    8, 0x08,
     107,    8,    8,    8, 0x08,
     133,    8,    8,    8, 0x08,
     159,    8,    8,    8, 0x08,
     185,    8,    8,    8, 0x08,
     211,    8,    8,    8, 0x08,
     237,    8,    8,    8, 0x08,
     263,    8,    8,    8, 0x08,
     289,    8,    8,    8, 0x08,
     319,    8,    8,    8, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_Dialpad[] = {
    "Dialpad\0\0text\0typed(QString)\0"
    "on_pushButton_1_clicked()\0"
    "on_pushButton_2_clicked()\0"
    "on_pushButton_3_clicked()\0"
    "on_pushButton_4_clicked()\0"
    "on_pushButton_5_clicked()\0"
    "on_pushButton_6_clicked()\0"
    "on_pushButton_7_clicked()\0"
    "on_pushButton_8_clicked()\0"
    "on_pushButton_9_clicked()\0"
    "on_pushButton_0_clicked()\0"
    "on_pushButton_diese_clicked()\0"
    "on_pushButton_etoile_clicked()\0"
};

const QMetaObject Dialpad::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_Dialpad,
      qt_meta_data_Dialpad, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &Dialpad::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *Dialpad::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *Dialpad::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_Dialpad))
        return static_cast<void*>(const_cast< Dialpad*>(this));
    return QWidget::qt_metacast(_clname);
}

int Dialpad::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: typed((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 1: on_pushButton_1_clicked(); break;
        case 2: on_pushButton_2_clicked(); break;
        case 3: on_pushButton_3_clicked(); break;
        case 4: on_pushButton_4_clicked(); break;
        case 5: on_pushButton_5_clicked(); break;
        case 6: on_pushButton_6_clicked(); break;
        case 7: on_pushButton_7_clicked(); break;
        case 8: on_pushButton_8_clicked(); break;
        case 9: on_pushButton_9_clicked(); break;
        case 10: on_pushButton_0_clicked(); break;
        case 11: on_pushButton_diese_clicked(); break;
        case 12: on_pushButton_etoile_clicked(); break;
        default: ;
        }
        _id -= 13;
    }
    return _id;
}

// SIGNAL 0
void Dialpad::typed(QString _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_END_MOC_NAMESPACE
