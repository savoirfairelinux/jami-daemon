/****************************************************************************
** Meta object code from reading C++ file 'dlghooks.h'
**
** Created: Tue Apr 20 14:19:37 2010
**      by: The Qt Meta Object Compiler version 62 (Qt 4.6.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/conf/dlghooks.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dlghooks.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.6.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_DlgHooks[] = {

 // content:
       4,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

static const char qt_meta_stringdata_DlgHooks[] = {
    "DlgHooks\0"
};

const QMetaObject DlgHooks::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DlgHooks,
      qt_meta_data_DlgHooks, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &DlgHooks::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *DlgHooks::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *DlgHooks::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DlgHooks))
        return static_cast<void*>(const_cast< DlgHooks*>(this));
    if (!strcmp(_clname, "Ui_DlgHooksBase"))
        return static_cast< Ui_DlgHooksBase*>(const_cast< DlgHooks*>(this));
    return QWidget::qt_metacast(_clname);
}

int DlgHooks::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    return _id;
}
QT_END_MOC_NAMESPACE
