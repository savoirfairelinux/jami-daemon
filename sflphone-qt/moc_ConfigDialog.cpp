/****************************************************************************
** Meta object code from reading C++ file 'ConfigDialog.h'
**
** Created: Wed Feb 18 14:02:53 2009
**      by: The Qt Meta Object Compiler version 59 (Qt 4.4.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "ConfigDialog.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ConfigDialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 59
#error "This file was generated using the moc from 4.4.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_ConfigurationDialog[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   10, // methods
       0,    0, // properties
       0,    0, // enums/sets

 // slots: signature, parameters, type, tag, flags
      21,   20,   20,   20, 0x08,
      59,   54,   20,   20, 0x08,
     112,   95,   20,   20, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_ConfigurationDialog[] = {
    "ConfigurationDialog\0\0"
    "on_buttonNouveauCompte_clicked()\0text\0"
    "on_edit1_Alias_textChanged(QString)\0"
    "current,previous\0"
    "on_listWidgetComptes_currentItemChanged(QListWidgetItem*,QListWidgetIt"
    "em*)\0"
};

const QMetaObject ConfigurationDialog::staticMetaObject = {
    { &QDialog::staticMetaObject, qt_meta_stringdata_ConfigurationDialog,
      qt_meta_data_ConfigurationDialog, 0 }
};

const QMetaObject *ConfigurationDialog::metaObject() const
{
    return &staticMetaObject;
}

void *ConfigurationDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_ConfigurationDialog))
        return static_cast<void*>(const_cast< ConfigurationDialog*>(this));
    return QDialog::qt_metacast(_clname);
}

int ConfigurationDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: on_buttonNouveauCompte_clicked(); break;
        case 1: on_edit1_Alias_textChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: on_listWidgetComptes_currentItemChanged((*reinterpret_cast< QListWidgetItem*(*)>(_a[1])),(*reinterpret_cast< QListWidgetItem*(*)>(_a[2]))); break;
        }
        _id -= 3;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
