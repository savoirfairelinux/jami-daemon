/****************************************************************************
** Meta object code from reading C++ file 'dlgaccounts.h'
**
** Created: Tue Apr 20 14:19:37 2010
**      by: The Qt Meta Object Compiler version 62 (Qt 4.6.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/conf/dlgaccounts.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dlgaccounts.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.6.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_Private_AddCodecDialog[] = {

 // content:
       4,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
      30,   24,   23,   23, 0x05,

 // slots: signature, parameters, type, tag, flags
      48,   23,   23,   23, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_Private_AddCodecDialog[] = {
    "Private_AddCodecDialog\0\0alias\0"
    "addCodec(QString)\0emitNewCodec()\0"
};

const QMetaObject Private_AddCodecDialog::staticMetaObject = {
    { &KDialog::staticMetaObject, qt_meta_stringdata_Private_AddCodecDialog,
      qt_meta_data_Private_AddCodecDialog, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &Private_AddCodecDialog::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *Private_AddCodecDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *Private_AddCodecDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_Private_AddCodecDialog))
        return static_cast<void*>(const_cast< Private_AddCodecDialog*>(this));
    return KDialog::qt_metacast(_clname);
}

int Private_AddCodecDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = KDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: addCodec((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 1: emitNewCodec(); break;
        default: ;
        }
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void Private_AddCodecDialog::addCodec(QString _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
static const uint qt_meta_data_DlgAccounts[] = {

 // content:
       4,       // revision
       0,       // classname
       0,    0, // classinfo
      30,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
      13,   12,   12,   12, 0x05,

 // slots: signature, parameters, type, tag, flags
      29,   12,   12,   12, 0x0a,
      47,   12,   12,   12, 0x0a,
      70,   12,   65,   12, 0x0a,
      83,   12,   12,   12, 0x0a,
     100,   12,   12,   12, 0x0a,
     116,   12,   12,   12, 0x08,
     137,   12,   12,   12, 0x08,
     168,   12,   12,   12, 0x08,
     202,   12,   12,   12, 0x08,
     232,   12,   12,   12, 0x08,
     264,   12,   12,   12, 0x08,
     295,   12,   12,   12, 0x08,
     334,  329,   12,   12, 0x08,
     387,  370,   12,   12, 0x08,
     467,   12,   12,   12, 0x08,
     497,  489,   12,   12, 0x08,
     531,   12,   12,   12, 0x08,
     564,  559,   12,   12, 0x08,
     600,  489,   12,   12, 0x08,
     634,  628,   12,   12, 0x08,
     665,  660,   12,   12, 0x08,
     683,   12,   12,   12, 0x28,
     694,   12,   12,   12, 0x08,
     715,  709,   12,   12, 0x08,
     732,   12,   12,   12, 0x08,
     748,   12,   12,   12, 0x08,
     781,  767,   12,   12, 0x08,
     843,  833,   12,   12, 0x08,
     868,  833,   12,   12, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_DlgAccounts[] = {
    "DlgAccounts\0\0updateButtons()\0"
    "saveAccountList()\0loadAccountList()\0"
    "bool\0hasChanged()\0updateSettings()\0"
    "updateWidgets()\0changedAccountList()\0"
    "connectAccountsChangedSignal()\0"
    "disconnectAccountsChangedSignal()\0"
    "on_button_accountUp_clicked()\0"
    "on_button_accountDown_clicked()\0"
    "on_button_accountAdd_clicked()\0"
    "on_button_accountRemove_clicked()\0"
    "text\0on_edit1_alias_textChanged(QString)\0"
    "current,previous\0"
    "on_listWidget_accountList_currentItemChanged(QListWidgetItem*,QListWid"
    "getItem*)\0"
    "updateAccountStates()\0account\0"
    "addAccountToAccountList(Account*)\0"
    "updateAccountListCommands()\0item\0"
    "updateStatusLabel(QListWidgetItem*)\0"
    "updateStatusLabel(Account*)\0model\0"
    "codecClicked(QModelIndex)\0name\0"
    "addCodec(QString)\0addCodec()\0"
    "codecChanged()\0value\0updateCombo(int)\0"
    "addCredential()\0removeCredential()\0"
    "item,previous\0"
    "selectCredential(QListWidgetItem*,QListWidgetItem*)\0"
    "accountId\0loadCredentails(QString)\0"
    "saveCredential(QString)\0"
};

const QMetaObject DlgAccounts::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DlgAccounts,
      qt_meta_data_DlgAccounts, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &DlgAccounts::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *DlgAccounts::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *DlgAccounts::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_DlgAccounts))
        return static_cast<void*>(const_cast< DlgAccounts*>(this));
    if (!strcmp(_clname, "Ui_DlgAccountsBase"))
        return static_cast< Ui_DlgAccountsBase*>(const_cast< DlgAccounts*>(this));
    return QWidget::qt_metacast(_clname);
}

int DlgAccounts::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: updateButtons(); break;
        case 1: saveAccountList(); break;
        case 2: loadAccountList(); break;
        case 3: { bool _r = hasChanged();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 4: updateSettings(); break;
        case 5: updateWidgets(); break;
        case 6: changedAccountList(); break;
        case 7: connectAccountsChangedSignal(); break;
        case 8: disconnectAccountsChangedSignal(); break;
        case 9: on_button_accountUp_clicked(); break;
        case 10: on_button_accountDown_clicked(); break;
        case 11: on_button_accountAdd_clicked(); break;
        case 12: on_button_accountRemove_clicked(); break;
        case 13: on_edit1_alias_textChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 14: on_listWidget_accountList_currentItemChanged((*reinterpret_cast< QListWidgetItem*(*)>(_a[1])),(*reinterpret_cast< QListWidgetItem*(*)>(_a[2]))); break;
        case 15: updateAccountStates(); break;
        case 16: addAccountToAccountList((*reinterpret_cast< Account*(*)>(_a[1]))); break;
        case 17: updateAccountListCommands(); break;
        case 18: updateStatusLabel((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 19: updateStatusLabel((*reinterpret_cast< Account*(*)>(_a[1]))); break;
        case 20: codecClicked((*reinterpret_cast< const QModelIndex(*)>(_a[1]))); break;
        case 21: addCodec((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 22: addCodec(); break;
        case 23: codecChanged(); break;
        case 24: updateCombo((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 25: addCredential(); break;
        case 26: removeCredential(); break;
        case 27: selectCredential((*reinterpret_cast< QListWidgetItem*(*)>(_a[1])),(*reinterpret_cast< QListWidgetItem*(*)>(_a[2]))); break;
        case 28: loadCredentails((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 29: saveCredential((*reinterpret_cast< QString(*)>(_a[1]))); break;
        default: ;
        }
        _id -= 30;
    }
    return _id;
}

// SIGNAL 0
void DlgAccounts::updateButtons()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}
QT_END_MOC_NAMESPACE
