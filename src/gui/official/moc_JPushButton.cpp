/****************************************************************************
** Meta object code from reading C++ file 'JPushButton.hpp'
**
** Created: Wed Sep 21 13:46:03 2005
**      by: The Qt Meta Object Compiler version 58 (Qt 4.0.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "JPushButton.hpp"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'JPushButton.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 58
#error "This file was generated using the moc from 4.0.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

static const uint qt_meta_data_JPushButton[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   10, // methods
       0,    0, // properties
       0,    0, // enums/sets

 // signals: signature, parameters, type, tag, flags
      13,   12,   12,   12, 0x05,

       0        // eod
};

static const char qt_meta_stringdata_JPushButton[] = {
    "JPushButton\0\0clicked()\0"
};

const QMetaObject JPushButton::staticMetaObject = {
    { &QLabel::staticMetaObject, qt_meta_stringdata_JPushButton,
      qt_meta_data_JPushButton, 0 }
};

const QMetaObject *JPushButton::metaObject() const
{
    return &staticMetaObject;
}

void *JPushButton::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_JPushButton))
	return static_cast<void*>(const_cast<JPushButton*>(this));
    return QLabel::qt_metacast(_clname);
}

int JPushButton::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QLabel::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: clicked(); break;
        }
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void JPushButton::clicked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}
