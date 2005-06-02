/****************************************************************************
** Form implementation generated from reading ui file 'gui/qt/configurationpanel.ui'
**
** Created: Thu Jun 2 11:14:54 2005
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "gui/qt/configurationpanelui.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qframe.h>
#include <qlistbox.h>
#include <qlabel.h>
#include <qtabwidget.h>
#include <qwidget.h>
#include <qgroupbox.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

#include "configurationpanel.ui.h"
static const char* const image0_data[] = { 
"312 91 104 2",
".R c #626562",
".H c #6a656a",
".G c #6a696a",
".F c #6a6d6a",
".Y c #736d73",
"#a c #737173",
".1 c #737573",
"#m c #7b757b",
"#B c #7b797b",
"#x c #7b7d7b",
"#z c #837d83",
".I c #838183",
"#y c #838583",
".g c #83b2b4",
"#n c #8b858b",
"#q c #8b898b",
".u c #8b8d8b",
"#p c #8bb2b4",
".h c #8bb2bd",
".f c #8bb6bd",
".E c #948d94",
"#e c #949194",
"#f c #949594",
".z c #94b6bd",
".2 c #94babd",
".M c #94bac5",
"#G c #94bebd",
"#D c #94bec5",
".v c #9c959c",
".t c #9c999c",
".s c #9c9d9c",
".i c #9cbec5",
"#d c #9cc2c5",
"#C c #a49da4",
"#i c #a4a1a4",
"## c #a4a5a4",
".e c #a4c2c5",
"#v c #a4c2cd",
".A c #a4c6cd",
".P c #aca5ac",
".r c #acaaac",
".S c #acaeac",
"#w c #acc6cd",
".j c #accacd",
"#c c #accad5",
"#K c #accecd",
".0 c #b4aeb4",
".q c #b4b2b4",
".6 c #b4b6b4",
"#J c #b4cad5",
".T c #b4ced5",
".L c #b4d2d5",
".7 c #bdb6bd",
".p c #bdbabd",
".w c #bdbebd",
".3 c #bdd2d5",
"#A c #bdd2de",
".4 c #bdd6de",
"#E c #c5bec5",
".o c #c5c2c5",
".x c #c5c6c5",
".d c #c5d6de",
".k c #c5dade",
"#g c #cdc6cd",
"#o c #cdcacd",
".n c #cdcecd",
"#j c #cddade",
".N c #cddae6",
".5 c #cddede",
"#h c #cddee6",
"#H c #cde2e6",
"#u c #d5ced5",
".y c #d5d2d5",
".J c #d5d6d5",
".W c #d5e2e6",
"#L c #d5e2ee",
".c c #d5e6e6",
".U c #d5e6ee",
".D c #ded6de",
"#. c #dedade",
".m c #dedede",
".B c #dee6ee",
"#b c #deeaee",
".8 c #e6dee6",
".Q c #e6e2e6",
".9 c #e6e6e6",
"#F c #e6eaee",
"#I c #e6eaf6",
"#k c #e6eeee",
".O c #e6eef6",
"#t c #eee6ee",
".X c #eeeaee",
"#r c #eeeeee",
".V c #eeeef6",
".l c #eef2f6",
".b c #eef6f6",
".Z c #f6eef6",
".K c #f6f2f6",
".C c #f6f6f6",
".a c #f6f6ff",
"#l c #f6faff",
"#s c #fff6ff",
".# c #fffaff",
"Qt c #ffffff",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.a.a.bQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.c.d.e.f.g.h.g.h.g.i.j.k.lQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.m.n.o.p.q.r.s.t.u.v.s.r.q.w.x.y.mQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.a.k.z.f.g.f.h.f.g.f.h.f.g.f.h.f.A.BQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.C.D.q.E.F.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.G.I.t.p.J.KQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.L.f.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.M.BQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.a.k.N.OQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.K.p.P.n.#QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.SQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.l.i.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.f.TQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.l.U.k.L.e.i.i.j.T.N.VQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.W.h.f.g.f.OQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.X.G.G.H.G.Y.ZQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.0.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.1QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.W.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.2.bQtQtQtQtQtQtQtQtQtQtQtQtQt.a.j.h.g.h.g.h.g.h.g.h.g.h.2.3.lQtQtQtQtQtQtQtQtQtQt.#.W.d.4.d.4.d.4.d.4.d.4.d.4.d.4.d.4.d.5.OQtQtQtQt.e.h.g.h.g.dQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.6.G.R.G.R.G.pQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.1.G.R.G.R.G.R.G.R.G.1.7.n.8.9.Q#..D###a.R.G.R.G.R.G.R.G.R.G#.QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.W.g.f.h.f.g.f.h.f.g.f.f.j.j.3.k#b.B#c.h.f.g.f.h.f.g.f.2.aQtQtQtQtQtQtQtQtQtQt.#.L.h.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.T.#QtQtQtQtQtQtQt.a.h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.f.h.WQtQt.#.f.g.f.h.f#dQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#e.H.G.H.G.H.SQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.G.H.G.H.G.H.G.H.G#fQtQtQtQtQtQtQtQtQt.##g.F.G.H.G.H.G.H.G.H#gQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.W.g.h.g.h.g.h.g.h.g.f.5QtQtQtQtQtQt.V.M.h.g.h.g.h.g.h.g.h.f.lQtQtQtQtQtQtQtQtQt.j.g.h.g.h.g.3.B.aQt.a#b#h#d.h.g.h.g.h.MQtQtQtQtQtQtQt.d.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.5QtQt.#.g.h.g.h.g.iQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.u.G.R.G.R.G#iQtQtQtQtQtQtQtQtQtQtQtQtQtQt.X.R.G.R.G.R.G.R.G#a.CQtQtQtQtQtQtQtQtQtQtQt.p.R.G.R.G.R.G.R.G.0QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.O.g.f.h.f.g.f.h.f.g.f.kQtQtQtQtQtQt#b.h.f.g.f.h.f.g.f.h.f.g.f.iQtQtQtQtQtQtQtQt.k.g.f.h.f.A.aQtQtQtQtQtQtQtQt.l.4.f.f.h.f.WQtQtQtQtQtQt#c.g.f.h.f.g.f.h.2.j.d#h.W#h.W#h#h#j#h.k.W#kQtQtQt#l.f.h.f.g.f.iQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.E.H.G.H.G.H#iQtQtQtQtQtQtQtQtQtQtQtQtQtQt.##m.H.G.H.G.H.G.H.DQtQtQtQtQtQtQtQtQtQtQtQtQt#n.H.G.H.G.H.G.H#oQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.2.h.g.h.g.h.g.h.g.h.gQtQtQtQtQtQt#b#p.h.g.h.g.h.g.h.g.h.g.h.g.h.3QtQtQtQtQtQt.O.g.h.g.h.iQtQtQtQtQtQtQtQtQtQtQtQt.##h.T.dQtQtQtQtQtQtQt.A.h.g.h.g.h.g.4#lQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.g.h.g.h.g.iQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.u.G.R.G.R.G.tQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.p.G.R.G.R.G.F.rQtQtQtQtQtQtQtQtQtQtQtQtQtQt.m.1.G.G.R.G.R#nQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.3.f.h.f.g.f.h.f.g.f.h.fQtQtQtQtQtQt.2.f.g.f.h.f.g.f.h.f.g.f.h.f.g.f#kQtQtQtQtQt.3.f.h.f.g.lQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#c.h.f.g.f.h.dQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#l.f.g.f.h.f.iQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.E.H.G.H.G.H#qQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.n#..9.KQtQtQtQtQtQtQtQt#r.Q#sQtQtQtQtQtQtQtQtQt.C.X.m.y#tQtQtQtQtQtQtQtQtQtQtQtQtQt.#QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.9#u.x#..XQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQt.l.h.g.h.g.h.g.h.g.h.g.h.g.lQtQtQtQt.l.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g#vQtQtQtQtQt#v.g.h.g.2QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#w.h.g.h.g.iQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.g.h.g.h.g.iQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.K#o.n.9.#Qt.K.y.q.t#x.Y#y.P.o#tQtQtQtQtQtQtQtQt.u.G.R.G.R.G.R#gQtQt#..w#i#n#y#i.p.KQtQtQtQtQtQtQtQtQtQtQtQt.#.w.t#y.G.G.R.G#a#q#i.9QtQtQtQtQtQtQtQtQtQtQt.Z.o.w.y.X.#Qt#r.n.r#e.1.G.G.I.t#tQtQtQtQtQtQtQtQtQtQtQt#s###z.G.G.R.G.R.G.F##.QQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQt#A.h.f.g.f.h.f.g.f.h.f.g.f.TQtQtQtQtQt.e.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.VQtQtQtQt.f.f.g.f.AQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#c.g.f.h.f.LQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#l.f.h.f.g.f.iQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.q.G.H.G.H.G#a.G.H.G.H.G.H.G.H.G.H.0QtQtQtQtQtQtQt.E.H.G.H.G.H.G.H#a.G.G.H.G.H.G.H.G#a#uQtQtQtQtQtQtQtQtQtQt.x#B.H.G.H.G.H.G.H.G.H.G.G#C.XQtQtQtQtQtQtQtQt#i.H.G.H.G.H.G#a.G.H.G.H.G.H.G.H.G.G.rQtQtQtQtQtQtQtQtQt#g#a.G.H.G.H.G.H.G.H.G.H.G.t.#QtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQt.f.h.g.h.g.h.g.h.g.h.g.h.g.f.#QtQtQtQt#l.2.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.jQtQtQt.l.h.g.h.g#DQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.A.h.g.h.g#hQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.g.h.g.h.g.iQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.1.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R#q#rQtQtQtQtQt.u.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R#EQtQtQtQtQtQtQt.8.I.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G#i.#QtQtQtQtQt#s.G.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R#gQtQtQtQtQtQt#r#q.R.G.R.G.R.G.R.G.R.G.R.G.R.G#x.ZQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQt#h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.VQtQtQtQtQt.a.i.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f#lQtQtQt.f.f.h.f.g.lQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#c.h.f.g.f#hQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#l.f.g.f.h.f#DQtQtQtQtQtQtQtQtQtQtQtQtQtQt#t.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H#m.CQtQtQtQt.E.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G#a.#QtQtQtQtQt#..G.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.EQtQtQtQtQt.y.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G#a#sQtQtQtQt.##n.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.I.#QtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQt#c.g.h.g.h.g.h.g.h.g.h.g.h.g.2.CQtQtQtQtQtQtQt.3.h.g.h.g.h.g.h.g.h.g.h.g.h.g.BQtQtQt.A.g.h.g.h.2.#QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#w.h.g.h.g#AQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.g.h.g.h.g.iQtQtQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.G.R.G#B##.p.s#a.G.R.G.R.G.R.rQtQtQtQt.u.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.qQtQtQtQt#t.F.G.R.G.R.G.R.G.R.Y#B.G.R.G.R.G.R.G.R.G.tQtQtQtQt.y.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.nQtQtQtQt#i.R.G.R.G.R.G.R#B#i.q#e.G.R.G.R.G.R.G##QtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQt.2.f.h.f.g.f.h.f.g.f.h.f.g.f#FQtQtQtQtQtQtQtQtQt.O#v.h.f.g.f.h.f.g.f.h.f.g.f.4QtQtQt.4.f.g.f.h.f.2.BQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#c.g.f.h.f.iQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#l.f.h.f.g.f.MQtQtQtQtQtQtQtQtQtQtQtQtQtQt#t.H.G.H.G.H.G.H.G.rQtQtQtQtQt.E.G.H.G.H.G.G.ZQtQtQt.E.H.G.H.G.H.G.H.G#f.p.S#z.H.G.H.G.H.G.H.EQtQtQtQt.u.G.H.G.H.G.H.G.r.XQtQt.C.D.I.G.H.G.H.G.H.G.yQtQtQt.y.G.H.G.H.G.H.G.H#z##.p#f.G.H.G.H.G.H.G.H.pQtQtQt.X.G.G.H.G.H.G#x.QQtQtQtQt.Q#B.H.G.H.G.H.Y.CQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQt#l.h.g.h.g.h.g.h.g.h.g.h.g.i.OQtQtQtQtQtQtQtQtQtQtQtQt#h.f.h.g.h.g.h.g.h.g.h.g.AQtQtQtQt#d.h.g.h.g.h.g#d.k.OQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.A.h.g.h.g.h.kQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.g.h.g.h.g.iQtQtQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.G.0QtQtQtQtQtQtQt#q.G.R.G.R.G.tQtQtQt.u.G.R.G.R.G.R.G#oQtQtQt.C#y.R.G.R.G.R.G#BQtQtQt.n.G.R.G.R.G.R#a.yQtQtQtQtQtQt.##f.G.R.G.R.G.R#BQtQtQt.y.R.G.R.G.R.G.R#e.#QtQtQt#o.G.R.G.R.G.R.G#iQtQtQt.t.G.R.G.R.G.H.ZQtQtQtQtQtQt.Q.G.R.G.R.G.R#EQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQt.O.h.f.g.f.h.f.g.f.h.f.g.dQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.a.i.f.g.f.h.f.g.f.h.f.fQtQtQtQt.#.M.f.g.f.h.f.g.f.h.f.e.d.B#lQtQtQtQtQtQtQtQtQtQtQtQtQt#c.h.f.g.f.h.f.3QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#l.f.g.f.h.f.2QtQtQtQtQtQtQtQtQtQtQtQtQtQt#t.H.G.H.G.H.G#a.#QtQtQtQtQtQtQt.D.H.G.H.G.H.GQtQtQt.E.H.G.H.G.H.G.SQtQtQtQtQt.X.G.H.G.H.G.H.GQtQtQt.t.H.G.H.G.H.G.pQtQtQtQtQtQtQtQtQt#B.G.H.G.H.G.H.QQtQt.y.G.H.G.H.G.H.Y.CQtQtQtQtQt##.G.H.G.H.G.H#eQtQtQt#a.H.G.H.G.H#zQtQtQtQtQtQtQtQt#x.G.H.G.H.G#zQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQt.c.h.g.h.g.h.g.h.g.h#G.OQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.j.h.g.h.g.h.g.h.g.fQtQtQtQtQt#l.3.f.g.h.g.h.g.h.g.h.g.h.g.2.j.WQtQtQtQtQtQtQtQtQtQt#w.h.g.h.g.h.g.h.f#w.T.k.k.k.k.k.k.k.k.B.bQtQtQtQt.#.g.h.g.h.g#DQtQtQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.6QtQtQtQtQtQtQtQtQt#x.R.G.R.G.R.XQtQt.u.G.R.G.R.G.G.KQtQtQtQtQtQt.u.G.R.G.R.G.R.KQtQt.F.G.R.G.R.G#BQtQtQtQtQtQtQtQtQtQt#g.R.G.R.G.R.G.SQtQt.y.R.G.R.G.R.G#fQtQtQtQtQtQt.Q.R.G.R.G.R.G.uQtQt.8.R.G.R.G.R.G.R#.QtQtQtQtQtQt#..G.R.G.R.G.R.G.X"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQt#b.g.f.h.f.g.f.h.f.AQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#j.f.g.f.h.f.g.f.fQtQtQtQtQtQtQt.##h.i.f.h.f.g.f.h.f.g.f.h.f.g.M.kQtQtQtQtQtQtQtQt#c.g.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.bQtQtQt#l.f.h.f.g.f.MQtQtQtQtQtQtQtQtQtQtQtQtQtQt#t.H.G.H.G.H.G.QQtQtQtQtQtQtQtQtQt##.G.H.G.H.G.yQtQt.E.H.G.H.G.H#qQtQtQtQtQtQtQt#E.H.G.H.G.H.G#rQt.8.G.H.G.H.G.H.qQtQtQtQtQtQtQtQtQtQtQt#a.H.G.H.G.H.EQtQt.y.G.H.G.H.G.H.oQtQtQtQtQtQtQt.Y.H.G.H.G.H.EQtQt.p.G.H.G.H.G.H.G.G#z.s.p.r.v#x.G.H.G.H.G.H.G.H.y"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQt.B.h.g.h.g.h.g.h.AQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#H.h.g.h.g.h.g.hQtQtQtQtQtQtQtQtQtQt.#.O.5.T.i.f.g.h.g.h.g.h.g.h.f.OQtQtQtQtQtQt.A.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.lQtQtQt.#.g.h.g.h.g.MQtQtQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G.R#sQtQtQtQtQtQtQtQtQt.7.R.G.R.G.R.wQtQt.u.G.R.G.R.G#fQtQtQtQtQtQtQt#o.G.R.G.R.G.R.ZQt#u.R.G.R.G.R.G#gQtQtQtQtQtQtQtQtQtQtQt.I.G.R.G.R.G#xQtQt.y.R.G.R.G.R.G.nQtQtQtQtQtQtQt#B.G.R.G.R.G.uQtQt.r.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.w"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQt#I.h.f.g.f.h.f.2QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#v.4.aQtQtQtQt#h.f.g.f.h.f.fQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.l#h.j.g.f.h.f.g.f.2.aQtQtQtQtQt#c.h.f.g.f.h.f.g.M#w.4.d.k.d.k.d.k.d.k#H.lQtQtQtQt#l.f.g.f.h.f.2QtQtQtQtQtQtQtQtQtQtQtQtQtQt#t.H.G.H.G.H#aQtQtQtQtQtQtQtQtQtQt#o.G.H.G.H.G##QtQt.E.H.G.H.G.H.rQtQtQtQtQtQtQt.8.H.G.H.G.H.G#rQt.o.G.H.G.H.G.H.QQtQtQtQtQtQtQtQtQtQtQt.t.H.G.H.G.H#aQtQt.y.G.H.G.H.G.H.8QtQtQtQtQtQtQt#q.H.G.H.G.H.EQtQt.s.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H#u"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQt.l.h.g.h.g.h.g.OQtQtQtQt.#.O#H.WQtQtQtQtQtQtQtQtQtQt#d.h.g.A.WQtQtQt.L.h.g.h.g.AQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.l.T.h.g.h.g.h#DQtQtQtQtQt#w.h.g.h.g.h.g.k.#QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.g.h.g.h.g.MQtQtQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G#BQtQtQtQtQtQtQtQtQtQt.n.R.G.R.G.R.PQtQt.u.G.R.G.R.G.rQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.ZQt.p.R.G.R.G.R.G.XQtQtQtQtQtQtQtQtQtQtQt#i.G.R.G.R.G.GQtQt.y.R.G.R.G.R.G.QQtQtQtQtQtQtQt.u.G.R.G.R.G.uQtQt#e.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.G.E.#"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQt.f.f.h.f.g.TQtQtQt.B.j.f.g.f.iQtQtQtQtQtQtQtQtQtQt.T.h.f.g.f#d.aQt.#.f.f.g.f.3QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.4.f.h.f.g.f.CQtQtQtQt#c.g.f.h.f.g#hQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#l.f.h.f.g.f.2QtQtQtQtQtQtQtQtQtQtQtQtQtQt#t.H.G.H.G.H.FQtQtQtQtQtQtQtQtQtQt.o.G.H.G.H.G.pQtQt.E.H.G.H.G.H.0QtQtQtQtQtQtQt#t.H.G.H.G.H.G#rQt.o.G.H.G.H.G.H#.QtQtQtQtQtQtQtQtQtQtQt.E.H.G.H.G.H.IQtQt.y.G.H.G.H.G.H#tQtQtQtQtQtQtQt.E.H.G.H.G.H.EQtQt.t.G.H.G.H.G.H.G#z.P#o.y.y.y.y.y.y.y.y.y#..#QtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQt.j.g.h.g.h#kQt#b.z.g.h.g.h.g.TQtQtQtQtQtQtQtQtQtQt.3.h.g.h.g.h.f.WQt.T.g.h.g.WQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.j.h.g.h.g.BQtQtQtQt.A.h.g.h.g#DQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.g.h.g.h.g.fQtQtQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.KQtQtQtQtQtQtQtQtQt.S.R.G.R.G.R.nQtQt.u.G.R.G.R.G.rQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.ZQt.y.R.G.R.G.R.G.oQtQtQtQtQtQtQtQtQtQtQt.1.G.R.G.R.G.tQtQt.y.R.G.R.G.R.G.QQtQtQtQtQtQtQt.u.G.R.G.R.G.uQtQt.P.R.G.R.G.R.G#aQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQt.k.f.g.f.hQt#h.f.h.f.g.f.h.f#hQtQtQtQtQtQtQtQtQtQt#h.g.f.h.f.g.f.h.L.i.f.h.f.bQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.B.g.f.h.f.4QtQtQtQt#c.h.f.g.f.3QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#l.f.g.f.h.f.g.#QtQtQtQtQtQtQtQtQtQtQtQtQt#t.H.G.H.G.H.G#.QtQtQtQtQtQtQtQtQt#y.G.H.G.H.G.8QtQt.E.H.G.H.G.H.SQtQtQtQtQtQtQt#t.H.G.H.G.H.G#rQt.9.G.H.G.H.G.H#iQtQtQtQtQtQtQtQtQtQt#r.G.H.G.H.G.H.7QtQt.y.G.H.G.H.G.H#tQtQtQtQtQtQtQt.E.H.G.H.G.H.EQtQt.6.G.H.G.H.G.H.qQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQt.a.g.h.g.h.j.h.g.h.g.h.g.h.g.VQtQtQtQtQtQtQtQtQtQt.5.h.g.h.g.h.g.h.g.h.g.h.AQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.C.h.g.h.g.WQtQtQtQt#w.h.g.h.g#hQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.g.h.g.h.g.h#kQtQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G.R#CQtQtQtQtQtQtQtQt.C.G.R.G.R.G.R.KQtQt.u.G.R.G.R.G.rQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.ZQtQt.1.G.R.G.R.G.G.XQtQtQtQtQtQtQtQtQt#i.R.G.R.G.R.G#oQtQt.y.R.G.R.G.R.G.QQtQtQtQtQtQtQt.u.G.R.G.R.G.uQtQt.m.R.G.R.G.R.G#iQtQtQtQtQtQtQtQtQtQt.#.XQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQt#J.h.f.g.f.h.f.g.f.h.f.g.f.#QtQtQtQtQtQtQtQtQtQt.k.h.f.g.f.h.f.g.f.h.f.g#bQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.a.h.f.g.f.lQtQtQtQt#c.g.f.h.f#bQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#l.f.h.f.g.f.h.TQtQtQtQtQtQtQtQtQtQtQtQtQt#t.H.G.H.G.H.G.G.KQtQtQtQtQtQtQt.r.H.G.H.G.H#BQtQtQt.E.H.G.H.G.H.0QtQtQtQtQtQtQt#t.H.G.H.G.H.G#rQtQt.P.H.G.H.G.H.G.tQtQtQtQtQtQtQtQt.X.G.G.H.G.H.G.G.KQtQt.y.G.H.G.H.G.H#tQtQtQtQtQtQtQt.E.H.G.H.G.H.EQtQtQt#a.H.G.H.G.H#m.KQtQtQtQtQtQtQtQt.0.F.G#y.yQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQt#b.h.g.h.g.h.g.h.g.h.g.h.g.#QtQtQtQtQtQtQtQtQtQt.T.h.g.h.g.h.g.h.g.h.g.MQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.k.h.g.h.fQtQtQtQtQt.A.h.g.h.g.lQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.g.h.g.h.g.h.g#hQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.G#eQtQtQtQtQtQt#..R.G.R.G.R.G.pQtQtQt.u.G.R.G.R.G.rQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.ZQtQt.9.G.R.G.R.G.R.G#iQtQtQtQtQtQt.8.1.G.R.G.R.G.R#CQtQtQt.y.R.G.R.G.R.G.QQtQtQtQtQtQtQt.u.G.R.G.R.G.uQtQtQt##.G.R.G.R.G.R#q.CQtQtQtQtQt#r.E.R.G.R.G#a.CQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.A.f.h.f.g.f.h.f.g.f.h.f.aQtQtQtQtQtQtQtQtQtQt#v.g.f.h.f.g.f.h.f.g.f.cQtQtQtQtQtQt.O.lQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.C.M.g.f.h#cQtQtQtQtQt#c.h.f.g.f.OQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#l.f.g.f.h.f.g.f.h#v.k#h.W#b#F.l.bQtQtQtQt#t.H.G.H.G.H.G.H.G.u.ZQtQtQt.D#m.G.H.G.H.G#a.#QtQtQt.E.H.G.H.G.H.SQtQtQtQtQtQtQt#t.H.G.H.G.H.G#rQtQtQt.s.G.H.G.H.G.H.G#q.x.9.#.D.q.G.G.H.G.H.G.H.Y.9QtQtQt.y.G.H.G.H.G.H#tQtQtQtQtQtQtQt.E.H.G.H.G.H.EQtQtQt.K.F.G.H.G.H.G.H#m.o.X.C#..7#a.H.G.H.G.H.G.JQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.a.f.h.g.h.g.h.g.h.g.h.g.UQtQtQtQtQtQtQtQtQt.l.g.h.g.h.g.h.g.h.g.h#wQtQtQtQtQtQt#d.g.h.2.WQtQtQtQtQtQtQtQtQtQtQtQt.l.M.g.h.g.M.CQtQtQtQtQt#w.h.g.h.g.OQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g#v#hQt.Q.G.R.G.R.G.R.G.R.G.R#z#e#x.R.G.R.G.R.G.R.6QtQtQtQt.u.G.R.G.R.G.rQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.ZQtQtQt.C#B.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.wQtQtQtQt.y.R.G.R.G.R.G.QQtQtQtQtQtQtQt.u.G.R.G.R.G.uQtQtQtQt.o.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G#xQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.U.g.f.h.f.g.f.h.f.g.f.iQtQtQtQtQtQtQtQtQt.4.f.h.f.g.f.h.f.g.f.f#lQtQtQtQtQtQt.M.f.g.f.h.M.4.aQtQtQtQtQtQtQt.a.3.f.g.f.h.f.OQtQtQtQtQtQt#c.g.f.h.f.OQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.z.h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.a#t.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H#q#sQtQtQtQt.t.H.G.H.G.H.qQtQtQtQtQtQtQt.X.H.G.H.G.H.G.KQtQtQtQt.X.1.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.rQtQtQtQtQt.y.G.H.G.H.G.H#tQtQtQtQtQtQtQt.E.H.G.H.G.H.EQtQtQtQtQt.0.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G#B.ZQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.L.g.h.g.h.g.h.g.h.g.h.kQtQtQtQtQtQtQt#F.f.g.h.g.h.g.h.g.h#p#FQtQtQtQtQtQtQt.l#D.h.g.h.g.h.g.i#w.4.5.k#K.e.g.h.g.h.g.f.cQtQtQtQtQtQtQt.j.h.g.h.g.#QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#w.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.fQt.Q.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R.G.R#C.#QtQtQtQtQt.r.G.R.G.R.G.mQtQtQtQtQtQtQtQt#B.R.G.R.G.FQtQtQtQtQtQt#s##.G.R.G.R.G.R.G.R.G.R.G.R.G.R.F.oQtQtQtQtQtQt#s.G.G.R.G.R.G.CQtQtQtQtQtQtQt.S.G.R.G.R.G.sQtQtQtQtQtQt.y#B.G.R.G.R.G.R.G.R.G.R.G.R.G#y.ZQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.3.g.f.h.f.g.f.h.f.g.f.3QtQtQtQtQt.O.M.h.f.g.f.h.f.g.f.h#FQtQtQtQtQtQtQtQtQtQt#h.M.g.f.h.f.g.f.h.f.g.f.h.f.g.f.z.k#lQtQtQtQtQtQtQtQt.N.h.f.g.MQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.W.g.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.kQt#t.H.G.H.G.H.G.H.G.H.G.H.G.H.G.H.G.F#EQtQtQtQtQtQtQt.Z.Y.G.H.G.IQtQtQtQtQtQtQtQtQt.S.G.H.G.Y#gQtQtQtQtQtQtQtQt.Q#q.H.G.H.G.H.G.H.G.H.G.G.P.ZQtQtQtQtQtQtQtQt.v.H.G.H.G#iQtQtQtQtQtQtQtQt.X.G.G.H.G.G.8QtQtQtQtQtQtQtQt.q.Y.H.G.H.G.H.G.H.G.H#q.JQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.3.g.h.g.h.g.h.g.h.g.h.f#w.d.4.2.h.g.h.g.h.g.h.g.h.h.BQtQtQtQtQtQtQtQtQtQtQtQtQt.O.3.i.g.h.g.h.g.h.g.h.f.A.5QtQtQtQtQtQtQtQtQtQtQt#F.f.g.h.jQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.O#j.k.k.k.k.k.k.k.k.k.k.k.k.k.W#lQtQt.Q.G.R.G.R.G.R#B.J.K.m#g.S.t.r#o.9QtQtQtQtQtQtQtQtQtQt#s#o.p#g.#QtQtQtQtQtQtQtQtQtQt.y#E#oQtQtQtQtQtQtQtQtQtQtQtQt.8.x.S#e#B#a#z#q###o#sQtQtQtQtQtQtQtQtQtQtQt#..q.p.9QtQtQtQtQtQtQtQtQtQt.C.o.q#o.#QtQtQtQtQtQtQtQtQtQt.K#..w.P#q#y.t.q.x.QQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.W.f.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.f.h.f.g.f#d.bQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.C.O.c#L#F.aQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.l#L.WQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#t.H.G.H.G.H.G.7QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.a.A.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.g.h.4QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G.R.mQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#b#c.h.f.g.f.h.f.g.f.h.f.g.f.h.f.f.d.aQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#t.H.G.H.G.H.Y.#QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.a.4.A.f.h.g.h.g.h.g.h.2.T.5QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.Q.G.R.G.R.G#zQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#rQtQtQt.KQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.C#F.B#b.B#F.aQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.K.H.G.H.G.H.tQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.m.oQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.r.I#uQt.Q#n.vQtQtQtQtQtQtQtQtQtQtQtQt.x.XQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#z.R.G.R.G.SQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#o.u.XQtQtQt.P.I#sQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#e#z.7Qt#o#z.IQtQtQtQtQtQtQtQtQtQtQt.Z#x.qQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.6.G.H.G.F.8QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#e.I.6QtQtQt##.I.mQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.u.I.6Qt.x.I.IQtQtQtQtQtQtQtQtQtQtQt.m.I.SQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.9#u.y.KQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.9.I#z.EQtQtQt.P#x.xQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.E#x.6Qt#g#x.I.#QtQtQtQtQtQtQtQtQtQt.Q#z.qQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.#.y#E.w.w.w#.QtQtQtQt.K.K.#Qt.9.y.x.QQtQtQtQt.#.8.p.t#i.7.QQtQt.#.#QtQtQtQtQt#r.8QtQtQtQtQtQt.CQtQtQtQt.y.p.r.p.D.#QtQtQt.#.7.0#g.n#o#sQtQtQtQtQtQtQtQtQtQt.t.I.I.I.PQtQt##.I#n#o.9.D.o.y.#QtQtQtQtQt#.#E.P.7#u.KQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQt#s.q.v.I#q#C.p.KQtQtQt.Q#g.6#g.7#o.CQtQtQt.u.I.6Qt.x.I.IQtQtQtQtQtQtQtQtQtQtQt.X.I.pQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#e.P.QQt.#.x#z.qQtQt.Z.I#x.I.s.7.t.I#z#oQtQt#r.E#x##.n#o.S#C.CQt.r#iQtQtQtQt#s#q#x#EQtQtQtQt.K.u#.Qt.Z#C.u.p#.#E.u#f#rQtQt.D#z.I#x.I#z#.QtQtQtQtQtQtQtQtQt.#.u.I#x.I.u.#Qt###z.I.I.t.t#y#x#e#sQtQt.#.r#q.S.D.x.t#q.QQtQtQtQtQtQtQtQtQt"
"QtQtQtQt.Q#q.I.I.t.s#y#z.EQtQt.n.E.x.#Qt.X#e.E.XQtQt.u#z.7Qt.x#z.I.#QtQtQtQtQtQtQtQtQtQt.##x.xQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.KQtQtQtQtQt.E.I.KQt#t.I.I#gQtQtQt.p.I.sQtQt.r.I.tQtQtQtQtQtQtQt.o.I.#QtQtQt#..I.I#eQtQtQtQt#u.I#sQt#C.I.ZQtQtQt#s.I#eQtQt#o.I.I.I#n.yQtQtQtQtQtQtQtQtQtQtQt#u.I.I#n.ZQtQt##.I#y.QQtQt.9#q.I#gQtQt.6.I.yQtQtQtQt#e#y.KQtQtQtQtQtQtQtQt"
"QtQtQtQt.E.I#q.QQtQt.#.n#EQtQt#s.#QtQtQtQt#o.I.7QtQt.u.I.6Qt.x.I.IQtQtQtQtQtQtQtQtQtQtQtQt.E.QQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.t.I.JQt.Q.I#nQtQtQtQt.##y#yQtQt.u.I.tQtQtQtQtQtQtQt.9.I.QQtQtQt.p.I#z.I.CQtQtQt.r.sQt#.#x.tQtQtQtQtQt.P#x#uQt#o#x.I#e.ZQtQtQtQtQtQtQtQtQtQtQtQt.K.I#z##QtQtQt.P#x.rQtQtQtQt.0.I.SQt.Z#y.IQtQtQtQtQt.w.I.6QtQtQtQtQtQtQtQt"
"QtQtQt.x.I#z.JQtQtQtQtQtQtQtQtQtQtQtQtQtQt.J.I.tQtQt.E#x.6Qt#g#x.I.#QtQtQtQtQtQtQtQtQtQtQt.qQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.o.I.I#uQt#t.I.0QtQtQtQtQt#i.IQtQt.7.I.I.6.ZQtQtQtQtQtQt.u#oQtQtQt#i.I.I.I.QQtQtQt.E#gQt.7.I.I#tQtQtQt.K#y.I.qQt#o.I.I#oQtQtQtQtQtQtQtQtQtQtQtQtQtQt#f.I.pQtQtQt##.I.oQtQtQtQt.n.I.PQt.D.I.I.nQtQtQtQt.v.I.tQtQtQtQtQtQtQtQt"
"QtQtQt.P.I#qQtQtQtQtQtQtQtQtQtQtQtQtQtQt.X#e.I#eQtQt.u.I.6Qt.x.I.IQtQtQtQtQtQtQtQtQtQtQtQt#sQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.y.q.s.I#z.I#oQt.Q.I.nQtQtQtQtQt.S.I.#Qt.K.q.I.I#z#C.n.#QtQtQt.q#iQtQt.##y.7.o#q.oQtQt.X.I.KQt.P#z.I.I#i.x#g.s.t#f.yQt#o#z.I.KQtQtQtQtQtQtQtQtQtQtQtQtQtQt#C#x#oQtQtQt###z#.QtQtQtQt.Q.I#iQt.o.I#z.I.t.o#o.P#f.v.pQtQtQtQtQtQtQtQt"
"QtQtQt.E.I.tQtQtQtQtQtQtQtQtQtQtQt.X.w.r#q#x.I.uQtQt.u#z.7Qt.x#z.I.#QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.C.7.t.DQtQt.X#n.I#oQt#t.I.DQtQtQtQtQt.7.IQtQtQtQt#s.o.t.I.I#n.wQtQt.D.I.ZQt.o#iQtQt#g#eQtQt.0#CQtQt#C.I#q#..KQtQtQtQtQtQtQt#o.I.IQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.s.I#oQtQtQt##.I.KQtQtQtQtQt.I.PQt#E.I#n#o.KQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQt.E.I#CQtQtQtQtQtQtQtQtQt.D.t.p.ZQt.#.q.I.EQtQt.u.I.6Qt.x.I.IQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.s.I.ZQtQtQtQt.r.I#gQt.Q.I.yQtQtQtQtQt.6.I.#QtQtQtQtQtQt#r#i.I#x#.Qt.#.I.u.s.I#oQtQt.K.I#f#i.I#oQtQt.q#x.wQtQtQtQtQtQtQtQtQt#o#x.IQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.s#z#gQtQtQt.P#x.#QtQtQtQt.#.I#iQt.n.I#iQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQt.s.I#qQtQtQtQtQtQtQtQt#.#x.6QtQtQtQt#t.I#qQtQt.E#x.6Qt#g#x.I.#QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.K.I.tQtQtQtQtQt.r.I#oQt#t.I.DQtQtQtQtQt.p.IQtQtQtQtQtQtQtQt.Z.I.I#gQtQt.P.I.I.I.ZQtQtQt#C.I.I.I#sQtQt.n.I.rQtQtQtQtQtQtQtQtQt#o.I.IQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.s.I.pQtQtQt##.I.#QtQtQtQtQt.I.PQt.X.I#eQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQt.p.I.I.DQtQtQtQtQtQtQt.r.I.mQtQtQtQt#t.I.EQtQt.u.I.6Qt.x.I.IQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#r.I.I.CQtQtQt.X.I.I.xQt.Q.I.yQtQtQtQtQt.p.I.#Qt.CQtQtQtQtQt.Q.I#z.QQtQt#o.I#x.EQtQtQtQt.w.I#x#iQtQtQtQt#f.I.9QtQtQtQt.X#gQtQt.n#z.IQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.r#x#q.CQtQt###z.#QtQtQtQt.#.I#iQtQt.0#z#uQtQtQtQt.C.x.#QtQtQtQtQtQtQtQt"
"QtQtQt.K#q#x.E.XQtQtQt.8.w.#.r#z.pQtQtQtQt##.I#qQtQt#f#z.7Qt.n#z.I.#QtQtQtQtQtQtQtQtQtQtQt#rQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#i.I#C#..8.q#y.I.I#uQt.X.I#.QtQtQtQtQt.o#yQtQt.6.u.7.y.X#o.t.I.6QtQtQt.##q.I.6QtQtQtQt.9.I.I.DQtQtQtQt.X#e#q.7.X.Q#E#y#oQtQt#..I#qQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.o.I.I.PQtQt.P.I.#QtQtQtQtQt.I.rQtQt.C#i.I.0.8#t#g.E.SQtQtQtQtQtQtQtQtQt"
"QtQtQtQt.y#n.I#n#i.r#q.I##Qt.m.I#y.o.X#g#f.I.I#eQtQt#C.I.pQt.D.I.IQtQtQtQtQtQtQtQtQtQtQt.K.I.pQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.J.r#i.w.J#t.J#o.CQtQt#..#QtQtQtQtQt.K.8QtQtQt#o.r.t#y.u.s.JQtQtQtQtQt.X#g.#QtQtQtQtQt.Q.DQtQtQtQtQtQt.#.o.r#f##.6.XQtQtQt#s.6.nQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt#i.I#x.yQt.X.nQtQtQtQtQtQt#u.QQtQtQtQt.n.r.t.s.q#.QtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQt#t#i#y.I#y##.xQtQtQt.X#E.t.q#o.Q.m.n.mQtQt.8.r.XQt#s.6#gQtQtQtQtQtQtQtQtQtQtQtQt.q.QQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt.C.ZQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQt.KQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"};

static const unsigned char image1_data[] = { 
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0xad, 0x00, 0x00, 0x00, 0x30,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x63, 0x57, 0xfa, 0xde, 0x00, 0x00, 0x13,
    0x1e, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0xed, 0x5d, 0x7f, 0x68, 0x1b,
    0x57, 0x9e, 0xff, 0x3c, 0x73, 0x5b, 0xe4, 0xe0, 0x2d, 0x52, 0xd9, 0x16,
    0x29, 0xd0, 0x62, 0x97, 0x26, 0x58, 0xde, 0x63, 0x41, 0xda, 0x18, 0x1a,
    0x85, 0x0d, 0x91, 0x7a, 0x85, 0xad, 0x1c, 0x17, 0x3a, 0xba, 0x5d, 0x88,
    0xed, 0xdd, 0x52, 0x29, 0x49, 0xc9, 0x3a, 0x77, 0x4b, 0x6b, 0xa7, 0x70,
    0xe4, 0x47, 0x71, 0xec, 0x72, 0x24, 0x8e, 0x12, 0x72, 0x67, 0xef, 0xe2,
    0x24, 0x72, 0x30, 0xb5, 0x65, 0xc8, 0x46, 0x2a, 0x38, 0xb6, 0x73, 0x64,
    0xb1, 0x54, 0x7c, 0xc8, 0x5e, 0x92, 0x5a, 0x2a, 0x59, 0x2c, 0x19, 0xa7,
    0xd8, 0xc1, 0x59, 0x6c, 0xe3, 0x2e, 0xb6, 0xd0, 0x1a, 0xd7, 0x98, 0x83,
    0xef, 0xfd, 0x31, 0xd6, 0x48, 0x23, 0xcd, 0x48, 0x33, 0x92, 0xb3, 0x4d,
    0xb7, 0x7a, 0x60, 0x3e, 0x4d, 0xe7, 0xbd, 0xef, 0xfb, 0x7e, 0xdf, 0x7c,
    0xdf, 0xe7, 0x7d, 0xdf, 0x77, 0xde, 0x8c, 0xb0, 0x0a, 0xe0, 0x06, 0x80,
    0x9d, 0x42, 0x22, 0x22, 0x5a, 0x20, 0x22, 0x22, 0x0a, 0x6d, 0x23, 0x6d,
    0x11, 0xed, 0x74, 0x3f, 0x25, 0xfc, 0xfe, 0x62, 0xd1, 0x0e, 0x3a, 0x3c,
    0xbb, 0x48, 0xa1, 0x05, 0x22, 0x53, 0xcf, 0x18, 0xa1, 0x8d, 0x08, 0x6d,
    0xb7, 0x65, 0xd1, 0xda, 0x47, 0xd4, 0x72, 0x6f, 0x8a, 0x88, 0x88, 0xbe,
    0x6d, 0xc3, 0x4b, 0xf8, 0xdd, 0xc5, 0x82, 0x1a, 0x4e, 0xaf, 0x10, 0x0d,
    0xcf, 0x12, 0x69, 0x2f, 0xdc, 0x26, 0xdc, 0xda, 0x76, 0x4c, 0x95, 0xa8,
    0x1d, 0x22, 0x72, 0x0e, 0x51, 0xc9, 0x81, 0x4b, 0xa8, 0x1a, 0x55, 0x35,
    0x20, 0x22, 0xb2, 0xf6, 0xe5, 0x67, 0x54, 0xb5, 0x68, 0xed, 0x23, 0x5a,
    0x4c, 0x6c, 0x94, 0x42, 0x88, 0x12, 0x2a, 0x42, 0x45, 0x15, 0x69, 0x8b,
    0x8f, 0x4f, 0x0b, 0x61, 0x54, 0x35, 0xd8, 0x3e, 0x3e, 0x5d, 0x72, 0xdc,
    0x12, 0xe6, 0xc5, 0xbc, 0x15, 0xa6, 0x57, 0x56, 0xc9, 0xd4, 0xb3, 0xb3,
    0xcc, 0x9a, 0x0b, 0x4d, 0x3d, 0x63, 0xa5, 0x90, 0xa1, 0x84, 0x39, 0x31,
    0x67, 0x05, 0x5a, 0x21, 0xd2, 0x0e, 0x3d, 0x7d, 0x86, 0xcd, 0x44, 0xed,
    0x85, 0xdb, 0x25, 0xc7, 0x2d, 0xa1, 0x2c, 0xca, 0x5e, 0x98, 0x5e, 0x59,
    0x25, 0xed, 0x85, 0x02, 0x18, 0xb3, 0x7b, 0x98, 0xd0, 0x43, 0x84, 0x9e,
    0x31, 0x42, 0x0f, 0x91, 0xdd, 0xcf, 0x67, 0x0b, 0x6e, 0x47, 0x17, 0x29,
    0xb4, 0xa8, 0x5c, 0xce, 0x4e, 0x38, 0x2e, 0x11, 0x51, 0x57, 0xcf, 0x0d,
    0x72, 0x1e, 0x73, 0x66, 0xfd, 0x75, 0xba, 0x3b, 0x89, 0x66, 0x89, 0x16,
    0x17, 0x17, 0xbf, 0x33, 0x21, 0x09, 0x11, 0xd1, 0xf0, 0xd0, 0x30, 0xd1,
    0xc6, 0xce, 0x4d, 0xe8, 0x8d, 0xc4, 0x06, 0x75, 0xba, 0x3b, 0xc9, 0x79,
    0xcc, 0x49, 0xfb, 0x0f, 0x59, 0xc9, 0x79, 0xcc, 0x49, 0x34, 0x5b, 0x98,
    0xfc, 0xa7, 0xa1, 0x9f, 0x14, 0xee, 0x0c, 0xc3, 0x5e, 0xb8, 0x4d, 0x73,
    0xab, 0x44, 0x94, 0xa0, 0x9c, 0x25, 0x34, 0x47, 0x7f, 0x17, 0xc6, 0xa5,
    0x04, 0xd1, 0xd4, 0xad, 0x69, 0x32, 0xee, 0x33, 0xd1, 0xe9, 0x7d, 0xed,
    0x64, 0xdc, 0x67, 0x12, 0xfe, 0xa4, 0xfe, 0x7d, 0xba, 0xad, 0xfd, 0x3b,
    0xc1, 0xec, 0xa7, 0xdb, 0xda, 0x05, 0xfd, 0x77, 0xca, 0x31, 0x86, 0xdd,
    0xc3, 0x59, 0xe3, 0xf1, 0x9b, 0x53, 0x2d, 0x05, 0x4d, 0xe4, 0xa7, 0xa1,
    0x9f, 0x22, 0xa6, 0xdd, 0xd8, 0x22, 0xd5, 0x0c, 0xab, 0xa4, 0x38, 0x47,
    0xa7, 0x0a, 0x8a, 0x71, 0x0b, 0x71, 0x5c, 0xe7, 0x31, 0xa7, 0xe8, 0x46,
    0x58, 0x0f, 0xdb, 0xa9, 0xab, 0xe7, 0x06, 0xd1, 0x0a, 0x51, 0xff, 0x40,
    0x3f, 0xcd, 0xb9, 0xe7, 0xc8, 0x7a, 0xd8, 0x2e, 0xaa, 0x33, 0x15, 0x9e,
    0x7a, 0xe6, 0x19, 0x97, 0x88, 0xd7, 0x9f, 0x56, 0x76, 0xc6, 0x21, 0x68,
    0x85, 0x44, 0x63, 0x60, 0xdc, 0x67, 0xe2, 0x99, 0xb6, 0xc0, 0x09, 0xbc,
    0xd3, 0xfa, 0x29, 0x66, 0x5a, 0xeb, 0x7d, 0x95, 0x31, 0x68, 0x20, 0xbf,
    0xc3, 0x9a, 0x06, 0x42, 0x45, 0xc5, 0xb8, 0xd6, 0xbe, 0x31, 0xc5, 0x0e,
    0x45, 0x44, 0x22, 0x46, 0x1d, 0xf3, 0x84, 0x64, 0x07, 0x90, 0x66, 0x89,
    0x36, 0xc2, 0x1b, 0x34, 0x3c, 0x34, 0x9c, 0x53, 0x3e, 0xcd, 0xd2, 0x8e,
    0x84, 0x2a, 0x39, 0xaf, 0x17, 0xc1, 0x4c, 0xb4, 0x48, 0xaa, 0x99, 0x8d,
    0x88, 0x68, 0xff, 0x21, 0x6b, 0xd6, 0x0a, 0x34, 0x37, 0x3a, 0x97, 0x73,
    0xbc, 0xfe, 0x1e, 0xfa, 0xd1, 0x62, 0xee, 0xf1, 0x66, 0x37, 0x00, 0x70,
    0x00, 0xfc, 0x00, 0xec, 0x09, 0xa2, 0xdd, 0x57, 0x7c, 0x00, 0x1c, 0x00,
    0x94, 0xa1, 0xb3, 0x36, 0x0c, 0xcf, 0x5b, 0x66, 0xc8, 0x15, 0xd7, 0xdd,
    0x30, 0x7a, 0x1f, 0x98, 0x15, 0xcb, 0x93, 0xc3, 0xfe, 0x77, 0xe6, 0x61,
    0xff, 0xc9, 0xab, 0xcc, 0x9f, 0xa6, 0xaf, 0x14, 0xce, 0x87, 0xa7, 0xa8,
    0xe9, 0xf8, 0x51, 0xa1, 0x7f, 0xab, 0xd5, 0x86, 0x8e, 0x4e, 0x77, 0xde,
    0x76, 0xe9, 0xe8, 0x5a, 0x21, 0x72, 0xfc, 0xb6, 0x11, 0xb1, 0x47, 0x31,
    0x49, 0x9b, 0xac, 0x56, 0x1b, 0x5a, 0x8f, 0x35, 0x43, 0xbb, 0xf7, 0x55,
    0x56, 0x77, 0xc8, 0x4a, 0xf1, 0xf5, 0x38, 0xb4, 0x15, 0x5a, 0x4c, 0xdc,
    0x0a, 0xc0, 0xf3, 0x12, 0x63, 0xe9, 0xf2, 0x22, 0xe3, 0x21, 0x3a, 0x73,
    0xee, 0x0c, 0xe2, 0xeb, 0x71, 0x18, 0xf7, 0x18, 0x61, 0x1f, 0xf4, 0xb2,
    0xa4, 0x9e, 0x9e, 0xc1, 0x4d, 0x44, 0x1e, 0xf0, 0xd7, 0xd2, 0x8b, 0x71,
    0x8f, 0x11, 0xfb, 0xf7, 0x99, 0xd0, 0xfa, 0x61, 0x2b, 0x3c, 0x2c, 0x25,
    0xcf, 0xe6, 0x9e, 0xa3, 0xba, 0x41, 0x07, 0x00, 0xa0, 0xeb, 0x72, 0x17,
    0x4c, 0x07, 0x0f, 0x30, 0x6f, 0xcf, 0x0d, 0xea, 0xbe, 0xde, 0x2d, 0xb4,
    0xd5, 0xeb, 0x0d, 0x58, 0x5e, 0x5e, 0x02, 0x00, 0x4c, 0x7d, 0x10, 0x82,
    0xb7, 0x71, 0x17, 0x93, 0xb2, 0x33, 0xa9, 0xb7, 0x54, 0x89, 0x7a, 0xc3,
    0xf0, 0xec, 0x65, 0xcc, 0x35, 0x4b, 0x64, 0xfb, 0xb0, 0x4e, 0x90, 0x97,
    0x5e, 0xb4, 0x15, 0x5a, 0x74, 0x59, 0xbb, 0x60, 0x38, 0xa6, 0x83, 0x66,
    0xf7, 0x6e, 0xa6, 0x54, 0x3f, 0x6d, 0x85, 0x56, 0xb0, 0x37, 0xa9, 0x9f,
    0x8b, 0x88, 0xba, 0xaf, 0x79, 0xe0, 0xbb, 0xe3, 0xcf, 0xea, 0x4b, 0xaf,
    0x37, 0x60, 0xa0, 0xbe, 0x1f, 0xda, 0x23, 0x1a, 0x6c, 0x3e, 0xbf, 0x8b,
    0xf9, 0x01, 0x94, 0x09, 0x37, 0x8a, 0x88, 0x6a, 0x7e, 0xef, 0x03, 0x8c,
    0xdb, 0x8e, 0xa2, 0x10, 0xb9, 0x6a, 0x79, 0x87, 0x9d, 0x98, 0x07, 0x7a,
    0xd7, 0xcd, 0xaa, 0xe4, 0xc9, 0xe1, 0xc9, 0xc7, 0x55, 0xd0, 0x6d, 0x11,
    0xe5, 0x73, 0x38, 0xb3, 0x49, 0xac, 0x8f, 0x2e, 0xa8, 0x85, 0xe3, 0xb0,
    0x9d, 0x6c, 0xa3, 0x73, 0x14, 0x19, 0x0f, 0x91, 0x6b, 0x83, 0x28, 0x9f,
    0xe3, 0xd6, 0xd8, 0xcd, 0x88, 0x3d, 0x8a, 0x41, 0x5b, 0xa1, 0x45, 0xd4,
    0x1b, 0xe6, 0xff, 0xc6, 0xc3, 0xb0, 0x5a, 0x6d, 0xe0, 0xc0, 0x21, 0x18,
    0x0c, 0xa0, 0xf1, 0xb8, 0x0b, 0xba, 0x04, 0x11, 0x07, 0xfe, 0x26, 0xc5,
    0xd7, 0xe3, 0xb0, 0x39, 0xeb, 0xb2, 0xe4, 0xc5, 0x2e, 0xc6, 0x84, 0x9b,
    0xb4, 0xf6, 0xb7, 0x38, 0x38, 0x00, 0x1e, 0x77, 0x27, 0x35, 0x1d, 0x3f,
    0x8a, 0x60, 0xf0, 0x24, 0x6c, 0xeb, 0x36, 0xe8, 0xf5, 0x06, 0x74, 0x1e,
    0x69, 0xc7, 0x7e, 0x93, 0x09, 0xce, 0x0a, 0x17, 0x62, 0x8f, 0x62, 0xe8,
    0x1d, 0xf4, 0xa2, 0xa6, 0xd6, 0x0c, 0xd7, 0x6c, 0x4a, 0xdf, 0xba, 0x41,
    0x07, 0x38, 0x70, 0x00, 0x00, 0x23, 0x8c, 0xf0, 0x03, 0xa8, 0x5c, 0xd7,
    0x8b, 0xec, 0xb5, 0x2c, 0xef, 0x17, 0xfe, 0xfb, 0x93, 0xaf, 0xdc, 0xb2,
    0x76, 0x8e, 0x5d, 0x1e, 0x01, 0x00, 0x41, 0x5e, 0xb2, 0x74, 0x9d, 0xe8,
    0x02, 0xf4, 0xdb, 0xe3, 0xd0, 0x60, 0x86, 0x65, 0x79, 0x3f, 0xf4, 0x7a,
    0x03, 0xa2, 0xde, 0x30, 0xfa, 0xaf, 0xdd, 0x10, 0xc6, 0x21, 0xbe, 0x1e,
    0x47, 0xd3, 0x9d, 0x26, 0xf8, 0x8f, 0x8f, 0x42, 0x8d, 0x7e, 0xb6, 0x75,
    0x9b, 0x48, 0x3f, 0x17, 0x11, 0x59, 0xac, 0x36, 0x74, 0x5f, 0xef, 0xc6,
    0xf2, 0xf2, 0x12, 0x9a, 0xf5, 0xcd, 0xe0, 0xea, 0x39, 0x8c, 0x9c, 0xf7,
    0xc1, 0xb8, 0xc7, 0x08, 0xcb, 0xf2, 0x7e, 0xbc, 0x71, 0xfd, 0x0d, 0xbc,
    0xf1, 0x76, 0x1d, 0x74, 0xb3, 0xfc, 0xfd, 0x17, 0x98, 0xb6, 0x75, 0x68,
    0x8a, 0x7a, 0x23, 0x05, 0x30, 0xa2, 0x3e, 0x00, 0x3a, 0x6e, 0x83, 0x54,
    0x71, 0xcd, 0x03, 0xbd, 0x9f, 0x16, 0xc7, 0xb0, 0xe9, 0xd8, 0x7e, 0xc2,
    0x01, 0x43, 0x06, 0x93, 0x49, 0x32, 0xe5, 0x2c, 0x51, 0x4d, 0x83, 0xfc,
    0x64, 0x02, 0xf8, 0x19, 0x6f, 0xdc, 0x53, 0x05, 0xdb, 0x21, 0x1b, 0x1a,
    0x1b, 0x1a, 0x45, 0x8c, 0xe6, 0x22, 0xa2, 0x70, 0x24, 0x8c, 0x2a, 0xf3,
    0x4f, 0xb3, 0x18, 0xfa, 0xc0, 0x3e, 0x13, 0x25, 0x65, 0x44, 0xc7, 0xc3,
    0xe8, 0xfe, 0xd4, 0x83, 0x74, 0x26, 0x19, 0x1b, 0x1a, 0x11, 0x98, 0xc7,
    0x45, 0x44, 0x35, 0xb5, 0x29, 0x3d, 0xa2, 0xde, 0x30, 0x46, 0x66, 0x46,
    0x70, 0xea, 0xfc, 0x59, 0x41, 0x87, 0xe6, 0x63, 0x2e, 0xd8, 0x1b, 0x9b,
    0x44, 0xfd, 0x34, 0x0c, 0x6c, 0xd0, 0x4f, 0xaf, 0x1c, 0x48, 0xb5, 0x1b,
    0x0d, 0xc3, 0xf3, 0x12, 0x63, 0xee, 0xb4, 0xbe, 0x9b, 0x8f, 0x35, 0xa3,
    0xe1, 0xfd, 0xa3, 0xac, 0xf5, 0x98, 0x93, 0x26, 0x23, 0x11, 0x91, 0x6d,
    0xc6, 0x3d, 0x46, 0x38, 0x8f, 0x34, 0xc0, 0xf2, 0xf6, 0x61, 0x36, 0x3a,
    0xd0, 0x4f, 0x13, 0x5f, 0x56, 0x61, 0x33, 0xee, 0x01, 0x00, 0x98, 0xf7,
    0x59, 0xd0, 0x7c, 0xdc, 0x05, 0x0f, 0x63, 0x2c, 0xb3, 0x1f, 0xae, 0x9e,
    0x43, 0xd5, 0xc7, 0x67, 0xf3, 0x8e, 0x43, 0x96, 0x5d, 0xe3, 0x61, 0x78,
    0x76, 0xa9, 0xd7, 0xaf, 0xae, 0xbe, 0x0e, 0xae, 0xe3, 0x2e, 0x24, 0xaf,
    0x5b, 0xad, 0x36, 0x98, 0x3b, 0xdd, 0xa2, 0xfb, 0xeb, 0x6e, 0x6b, 0x27,
    0xff, 0x1d, 0x3f, 0x00, 0x60, 0xbf, 0xc9, 0x04, 0xcb, 0xf5, 0x5e, 0x56,
    0xc6, 0x01, 0xd0, 0x6d, 0x11, 0xf9, 0x51, 0x20, 0x23, 0xea, 0x6c, 0x88,
    0x7e, 0x9d, 0xe5, 0x13, 0x00, 0x00, 0x4f, 0x05, 0x8a, 0x66, 0xd8, 0x74,
    0x74, 0xdf, 0xf4, 0xc1, 0x45, 0xf9, 0x99, 0xd2, 0xb3, 0x97, 0xb1, 0xe8,
    0x83, 0x30, 0x9c, 0x47, 0x1a, 0x70, 0xba, 0xfe, 0x34, 0x7f, 0x43, 0x32,
    0x18, 0xc5, 0xb6, 0x6e, 0xc3, 0x64, 0x24, 0x82, 0x4f, 0xae, 0xb8, 0x51,
    0x53, 0x6b, 0x86, 0x7d, 0x7c, 0x55, 0x90, 0xeb, 0x61, 0x8c, 0xa5, 0xdf,
    0xa8, 0xf9, 0xf0, 0x14, 0x35, 0x84, 0x37, 0xc8, 0x71, 0xd8, 0x2e, 0xdc,
    0x14, 0x0e, 0x1c, 0x30, 0x03, 0x34, 0x1f, 0x77, 0x41, 0x5b, 0xa1, 0x4d,
    0xc9, 0xff, 0x4a, 0x23, 0xe8, 0x71, 0xe6, 0x7c, 0x87, 0xd0, 0x5f, 0xb3,
    0xbe, 0x19, 0x9e, 0xbd, 0x8c, 0xb9, 0xaf, 0x2d, 0x0b, 0xed, 0xa5, 0x1c,
    0x96, 0x03, 0xe0, 0x6d, 0xdc, 0xc5, 0xfa, 0x3f, 0xea, 0x4f, 0xb5, 0xed,
    0x6c, 0xcd, 0xd0, 0x1e, 0xe0, 0x8c, 0x1c, 0x5f, 0xbf, 0x9a, 0x13, 0xd9,
    0x37, 0x76, 0x79, 0x0c, 0xf6, 0x41, 0x2f, 0xb3, 0xbc, 0x7d, 0x98, 0xf9,
    0x01, 0x7c, 0x72, 0xc5, 0x8d, 0x60, 0xf0, 0x24, 0x0c, 0x91, 0x2a, 0x4c,
    0x46, 0x22, 0xe8, 0xbe, 0xde, 0x8d, 0x01, 0xef, 0x00, 0x38, 0x00, 0xe5,
    0xc6, 0xf2, 0x94, 0x3c, 0x70, 0xe8, 0xa8, 0x3f, 0x23, 0x1e, 0xc7, 0x8c,
    0x71, 0xd0, 0x85, 0x89, 0x1a, 0xc2, 0x1b, 0x64, 0xab, 0xaf, 0x13, 0x8d,
    0x67, 0x78, 0x26, 0x5c, 0x90, 0x7e, 0x78, 0x04, 0xc1, 0x61, 0x39, 0x70,
    0x58, 0xfe, 0xcb, 0x12, 0x5c, 0x2b, 0x44, 0xf3, 0xe1, 0x29, 0x72, 0x6d,
    0x10, 0xe9, 0xc6, 0x89, 0x8c, 0x30, 0x0a, 0xd7, 0x27, 0x23, 0x11, 0xb8,
    0x88, 0x88, 0xdd, 0x40, 0x61, 0xb1, 0x6c, 0x3a, 0x9a, 0x5e, 0x73, 0x20,
    0xdc, 0x00, 0xc9, 0x72, 0x69, 0x72, 0x1e, 0xa7, 0xfe, 0x58, 0x55, 0x90,
    0xdc, 0x62, 0x62, 0xdb, 0x4c, 0xdc, 0x5c, 0x5c, 0x24, 0x83, 0xd6, 0x80,
    0xc0, 0x17, 0x13, 0x30, 0xc6, 0x8c, 0x38, 0xf9, 0xf9, 0xc9, 0xac, 0x98,
    0x35, 0xfa, 0x20, 0x0c, 0x0f, 0xe3, 0x63, 0xb9, 0xe6, 0xeb, 0xad, 0x88,
    0x3c, 0x08, 0x67, 0xc5, 0x9b, 0xa2, 0xfa, 0xdb, 0x0c, 0xb3, 0xe6, 0xee,
    0xa4, 0xde, 0x41, 0x2f, 0x00, 0x9e, 0x3d, 0x27, 0x82, 0x01, 0xcc, 0x3f,
    0x9a, 0x47, 0xe3, 0x71, 0x97, 0xd0, 0xbe, 0xf3, 0x5c, 0x3b, 0x2c, 0x6f,
    0x1f, 0x66, 0x22, 0xa6, 0xde, 0xee, 0x4f, 0x4a, 0xdf, 0xc8, 0x78, 0x88,
    0x4e, 0x7e, 0x78, 0x52, 0x90, 0xe9, 0xfa, 0x3c, 0xa8, 0x88, 0xc9, 0x5a,
    0xbe, 0x88, 0x64, 0x31, 0xd5, 0xd2, 0x5f, 0xe6, 0x85, 0xeb, 0x1a, 0xad,
    0x0e, 0xdd, 0x17, 0x2f, 0xf1, 0x76, 0x6e, 0x10, 0xd5, 0x1c, 0xcc, 0x66,
    0xcc, 0xf4, 0x15, 0x4b, 0xcd, 0x38, 0xa8, 0xd5, 0x2f, 0xb3, 0xff, 0x7c,
    0x45, 0x5b, 0xa1, 0xc5, 0xd8, 0xd0, 0x08, 0x1f, 0xd3, 0xd6, 0x0d, 0xa2,
    0x28, 0x06, 0x8c, 0x7c, 0xe5, 0x93, 0xed, 0xa8, 0x75, 0x7f, 0x15, 0x3a,
    0xdf, 0x2c, 0x4e, 0x7e, 0x3a, 0xce, 0xc7, 0xab, 0x14, 0x3b, 0x6a, 0x3a,
    0x6a, 0x76, 0xef, 0x66, 0x9e, 0x5d, 0x8c, 0x99, 0x0e, 0x1e, 0x60, 0xa3,
    0xef, 0xbf, 0xc0, 0x6e, 0x0c, 0x7a, 0x59, 0xe7, 0xb9, 0x76, 0x11, 0x63,
    0x74, 0x5f, 0xf3, 0xc0, 0x74, 0x6b, 0x9a, 0x6a, 0x1a, 0xcc, 0x08, 0x06,
    0x03, 0x00, 0x80, 0xa9, 0x6b, 0x21, 0x44, 0x87, 0xc2, 0x88, 0x3e, 0x08,
    0xe3, 0xf4, 0x07, 0x2d, 0x92, 0x0c, 0xd3, 0x7a, 0xa4, 0x55, 0xf8, 0x7f,
    0xb6, 0x75, 0x1b, 0x6a, 0x6a, 0xcd, 0xa8, 0x6b, 0x70, 0x20, 0xbe, 0x1e,
    0x07, 0x07, 0x0e, 0x7a, 0xbd, 0x01, 0x49, 0xe6, 0x13, 0xb5, 0x8f, 0x84,
    0x65, 0xf5, 0xb5, 0xe9, 0x2d, 0xc0, 0x76, 0x7d, 0x4d, 0x45, 0xb9, 0xd0,
    0x2a, 0xd9, 0x3e, 0x93, 0xc9, 0x20, 0x5c, 0x17, 0xcb, 0x69, 0xf9, 0xf8,
    0x2c, 0xb3, 0x5c, 0xef, 0x65, 0x97, 0xae, 0xf7, 0x32, 0xcb, 0xf5, 0x5e,
    0xd6, 0xd1, 0xe9, 0x66, 0xc9, 0x89, 0xc2, 0xeb, 0x9f, 0x6d, 0x8f, 0x1f,
    0x80, 0xde, 0x3d, 0x4c, 0x35, 0x0d, 0x66, 0xe8, 0x82, 0x5a, 0x68, 0x2a,
    0xca, 0xf9, 0xb8, 0x3e, 0x6d, 0x1c, 0x52, 0xfd, 0xf1, 0x2b, 0x4e, 0x21,
    0xfa, 0x65, 0xf6, 0xcf, 0xd5, 0x73, 0x88, 0x8e, 0x87, 0xd1, 0xf2, 0x45,
    0x84, 0x85, 0xbe, 0x88, 0xb0, 0x4c, 0x74, 0x7d, 0x1e, 0x64, 0x9b, 0xcf,
    0xef, 0x62, 0x65, 0x9b, 0x89, 0x0d, 0x8a, 0x2c, 0x03, 0x88, 0x6d, 0x33,
    0x5a, 0x81, 0xe8, 0x8b, 0x65, 0xef, 0x30, 0x93, 0xa5, 0x75, 0x3f, 0x70,
    0xfb, 0x9f, 0x97, 0x00, 0x4d, 0xe1, 0xf2, 0x93, 0x78, 0x36, 0x98, 0x3f,
    0x44, 0x70, 0xd1, 0xf6, 0x12, 0x93, 0xa7, 0x5e, 0xdd, 0xbf, 0xd4, 0xc1,
    0x0f, 0xbf, 0xa0, 0xe7, 0x37, 0xeb, 0x6b, 0x68, 0xba, 0xd8, 0x04, 0x20,
    0xc5, 0x98, 0x5e, 0xf3, 0x2e, 0xb6, 0xb6, 0x9b, 0x31, 0x0f, 0x63, 0xcc,
    0xde, 0xd8, 0xc4, 0x26, 0xf4, 0x93, 0x42, 0x7d, 0x73, 0xb5, 0x19, 0x7e,
    0x00, 0x6b, 0xbb, 0x19, 0xe3, 0xea, 0xf9, 0xc1, 0x4f, 0x97, 0x97, 0xfc,
    0xf7, 0x40, 0x7d, 0xbf, 0xd0, 0xef, 0x92, 0x29, 0xc5, 0x7a, 0x9e, 0x41,
    0xaf, 0xec, 0x44, 0x73, 0x7c, 0xdc, 0x28, 0xb4, 0xbf, 0x54, 0xdb, 0x29,
    0x48, 0x4d, 0xca, 0xf7, 0xc7, 0xfc, 0x7c, 0xfd, 0x99, 0xcc, 0xfe, 0x94,
    0x4f, 0x64, 0x5e, 0xff, 0x54, 0xfb, 0xa4, 0x3d, 0xae, 0x15, 0xa2, 0x53,
    0x83, 0x7c, 0xdc, 0x1d, 0xa8, 0x08, 0x20, 0x70, 0x67, 0x04, 0x9e, 0xbd,
    0x4c, 0x18, 0x87, 0xc6, 0x86, 0x46, 0x91, 0x7d, 0xa8, 0x46, 0x41, 0xfa,
    0x99, 0x4d, 0xe2, 0xfe, 0x3b, 0x4e, 0x9c, 0x11, 0x31, 0x7d, 0xe6, 0x4a,
    0x99, 0xfc, 0x77, 0x19, 0x50, 0x8e, 0xa2, 0x19, 0xf0, 0x47, 0x23, 0x70,
    0x18, 0x0d, 0xc8, 0x55, 0x1c, 0x46, 0x03, 0xe8, 0x04, 0x30, 0xfc, 0xab,
    0xe2, 0x19, 0x77, 0xe0, 0xcf, 0xb9, 0x1d, 0xb6, 0xa6, 0xd6, 0x0c, 0xff,
    0xf1, 0x51, 0xd4, 0xd4, 0x9a, 0x51, 0xe5, 0x09, 0xd1, 0xe8, 0x40, 0x3f,
    0x1f, 0x23, 0x25, 0x88, 0x5c, 0x44, 0x14, 0x19, 0x0f, 0x91, 0xe9, 0xd6,
    0xb4, 0x68, 0x69, 0x6a, 0xd6, 0x37, 0xa3, 0xb5, 0x56, 0xcc, 0x98, 0x88,
    0x88, 0xe5, 0x9e, 0x39, 0xd5, 0x42, 0xc9, 0x94, 0x4c, 0x3a, 0xc3, 0xf8,
    0x01, 0x74, 0x9c, 0x3b, 0x23, 0xb2, 0x37, 0xc9, 0x20, 0xda, 0x0a, 0x2d,
    0x0c, 0x47, 0x74, 0x42, 0x3d, 0xcf, 0x55, 0x8f, 0x70, 0x3d, 0x18, 0x0c,
    0xa0, 0xca, 0x13, 0xa2, 0xe4, 0xae, 0x38, 0xe9, 0x30, 0x47, 0x8f, 0x34,
    0x90, 0xf1, 0x91, 0x51, 0x68, 0x6f, 0x3e, 0x56, 0xa3, 0x38, 0x66, 0x54,
    0xb3, 0x02, 0x61, 0x26, 0x43, 0xdf, 0x6d, 0x7b, 0x46, 0xfe, 0x34, 0x22,
    0xbb, 0x22, 0xb8, 0x88, 0xc8, 0xd1, 0xd0, 0x58, 0x70, 0x4c, 0x9b, 0x19,
    0x33, 0x8f, 0x9c, 0xf7, 0x09, 0xd7, 0x2d, 0xbf, 0xb4, 0x89, 0xb2, 0x25,
    0xba, 0x04, 0x91, 0x7d, 0x7c, 0x95, 0xea, 0x0e, 0x59, 0xa9, 0xfb, 0x6d,
    0x4f, 0x2a, 0x7b, 0xe0, 0x2c, 0x34, 0x6b, 0x90, 0xc4, 0x1f, 0x8d, 0x80,
    0x7e, 0x53, 0x07, 0xb5, 0x85, 0x5d, 0x0b, 0x00, 0xcb, 0xb6, 0x82, 0xfa,
    0x6d, 0x79, 0xcf, 0x01, 0xe3, 0x2b, 0xd2, 0x33, 0x52, 0x93, 0xd8, 0xa0,
    0xa6, 0xf7, 0x8f, 0xca, 0xe6, 0x58, 0xa5, 0x8a, 0x71, 0x8f, 0x11, 0x97,
    0xda, 0x3a, 0x50, 0xb5, 0xa7, 0x0a, 0x8e, 0x06, 0x71, 0x7e, 0x56, 0xaf,
    0x37, 0x60, 0x73, 0xfd, 0x1b, 0xc9, 0x98, 0x2e, 0x33, 0x06, 0x34, 0xb5,
    0x4d, 0x53, 0xd3, 0x9d, 0x26, 0x51, 0x9d, 0x64, 0x6c, 0x27, 0x9a, 0x58,
    0x2b, 0x44, 0x36, 0xa7, 0x74, 0xfe, 0x33, 0xbd, 0xe8, 0xf5, 0x06, 0x78,
    0x2e, 0x77, 0x41, 0xbb, 0x97, 0x8f, 0xe1, 0x3d, 0x69, 0xb9, 0x55, 0xa9,
    0x98, 0x51, 0xaf, 0x37, 0xa0, 0x71, 0x78, 0x34, 0x6f, 0x76, 0x25, 0x73,
    0x82, 0x0b, 0xf6, 0x24, 0x63, 0xfa, 0xed, 0x34, 0x54, 0xba, 0xcd, 0x4a,
    0xc6, 0x61, 0xe0, 0xb0, 0x5d, 0x98, 0xd4, 0x52, 0xfa, 0x25, 0x63, 0xf3,
    0x4c, 0x3d, 0xd2, 0xf7, 0x04, 0x72, 0xc5, 0xb8, 0xc7, 0x08, 0x9f, 0x77,
    0x00, 0x1e, 0xc6, 0x18, 0xeb, 0xba, 0x4f, 0x74, 0xf2, 0xee, 0x36, 0x93,
    0xc5, 0xd4, 0xa1, 0xfd, 0x9f, 0xc2, 0x18, 0x79, 0x47, 0x79, 0x20, 0x9d,
    0x59, 0x5a, 0x83, 0x51, 0xb8, 0xff, 0x5a, 0xa3, 0xba, 0x5f, 0xeb, 0x37,
    0x80, 0xef, 0x5d, 0x96, 0x73, 0x33, 0xe6, 0x5a, 0x21, 0xba, 0xf4, 0xe9,
    0x25, 0x18, 0x61, 0x44, 0x72, 0xa9, 0x03, 0xf8, 0x19, 0xed, 0x87, 0x1f,
    0xa7, 0xeb, 0x4f, 0x23, 0x86, 0x18, 0x3a, 0x8e, 0x9c, 0x81, 0x67, 0xaf,
    0x38, 0xdd, 0x35, 0xe0, 0x1d, 0x80, 0x76, 0x59, 0x0b, 0xff, 0x8c, 0x1f,
    0x5c, 0x35, 0x87, 0xd1, 0xe5, 0x20, 0x2e, 0x1d, 0x69, 0x47, 0x0c, 0x31,
    0x18, 0xd6, 0xab, 0xe0, 0x8f, 0xf9, 0xe1, 0xac, 0x6d, 0x40, 0xf9, 0x6b,
    0xe5, 0x58, 0x7b, 0x3e, 0xa5, 0x87, 0x2e, 0x41, 0x14, 0xfe, 0x9f, 0x28,
    0xba, 0xff, 0xe8, 0x06, 0x57, 0xcd, 0x21, 0x86, 0x18, 0x5c, 0x2d, 0xa7,
    0x64, 0xf5, 0x9c, 0x18, 0x1a, 0x26, 0xc3, 0x66, 0x95, 0x50, 0x3f, 0xd9,
    0x5f, 0x0c, 0x31, 0xb4, 0xfe, 0xaa, 0x35, 0xeb, 0x41, 0xc5, 0xc4, 0xd0,
    0x30, 0xe1, 0x11, 0x30, 0xba, 0x1c, 0x4c, 0x6d, 0xa6, 0x88, 0xc8, 0x75,
    0xdc, 0x05, 0xae, 0x9a, 0x43, 0x79, 0xad, 0x0e, 0xa6, 0x83, 0x07, 0x54,
    0x3f, 0x48, 0xe9, 0xfe, 0xcc, 0x03, 0x67, 0x6d, 0x03, 0xbc, 0xe6, 0x5d,
    0xa2, 0x71, 0xe8, 0xbe, 0xe6, 0x41, 0xe5, 0xba, 0x5e, 0xd0, 0x6b, 0x72,
    0x3d, 0x82, 0xd3, 0xf5, 0x2d, 0x28, 0x47, 0x39, 0x96, 0xd6, 0xd7, 0x84,
    0x71, 0x48, 0xb6, 0xcb, 0xa7, 0x1f, 0xf6, 0x20, 0x2d, 0xb6, 0xcf, 0x9e,
    0x40, 0x99, 0xfd, 0xf9, 0x67, 0xfc, 0x68, 0x7e, 0xb3, 0x05, 0x66, 0x7d,
    0x0d, 0xd6, 0x0e, 0xa6, 0xc6, 0x19, 0x05, 0x9d, 0x7b, 0x75, 0xf3, 0xa7,
    0xb6, 0x76, 0xa2, 0x84, 0xe6, 0x56, 0x09, 0xdd, 0xea, 0xfa, 0xd7, 0x5e,
    0x78, 0xf6, 0x0f, 0xb7, 0x94, 0xf0, 0x29, 0x9e, 0x3d, 0x50, 0x7b, 0x16,
    0xe0, 0x46, 0x94, 0xf2, 0x9e, 0xe6, 0x52, 0x5d, 0x12, 0x44, 0xed, 0x61,
    0x75, 0x7a, 0xe4, 0x32, 0x8c, 0xb6, 0x88, 0x16, 0x13, 0x44, 0x2d, 0xf7,
    0x88, 0xac, 0xf7, 0xf9, 0x97, 0x29, 0x17, 0x13, 0x54, 0x7a, 0x2b, 0xf8,
    0x1f, 0x04, 0x19, 0xda, 0x6e, 0x93, 0xa2, 0x58, 0x52, 0xe3, 0x03, 0x7d,
    0xc4, 0x3f, 0xb2, 0x7c, 0x5a, 0x65, 0x62, 0x09, 0x38, 0x70, 0x5d, 0x59,
    0x6c, 0x7b, 0xe3, 0x63, 0xf9, 0x27, 0x63, 0x1d, 0x57, 0x89, 0x1e, 0xc7,
    0x25, 0xf2, 0xc9, 0x7a, 0x07, 0x9a, 0xdf, 0xcf, 0xff, 0x44, 0x2d, 0x17,
    0x6a, 0xb6, 0x88, 0xca, 0x7f, 0x00, 0x4c, 0x3c, 0x59, 0x83, 0xe5, 0x65,
    0x9d, 0x64, 0x9e, 0xd5, 0x45, 0x44, 0xc9, 0xeb, 0x13, 0x4f, 0xd6, 0x60,
    0x7c, 0xe5, 0x05, 0xd5, 0x79, 0xe5, 0x5c, 0xfd, 0x6f, 0x3e, 0x97, 0x1d,
    0x1a, 0xc5, 0x16, 0x56, 0x49, 0xab, 0xd1, 0xc1, 0x3f, 0x13, 0x05, 0xaa,
    0x6b, 0x60, 0x8a, 0x2f, 0xc1, 0xb2, 0x77, 0x77, 0xd1, 0xfd, 0x7a, 0xef,
    0xcf, 0x51, 0x58, 0x5b, 0x85, 0xf9, 0xc9, 0x00, 0xba, 0xde, 0xb2, 0x01,
    0x58, 0x83, 0xe1, 0xa5, 0x94, 0x3d, 0x13, 0xb3, 0x8b, 0xe4, 0x9e, 0x34,
    0x00, 0x08, 0x00, 0xb0, 0x01, 0x08, 0xc0, 0xf6, 0x96, 0x0d, 0x2e, 0xcd,
    0x37, 0xd0, 0x6c, 0x9f, 0x15, 0x50, 0xd3, 0xdf, 0xd2, 0xca, 0x2a, 0xf9,
    0xa1, 0x43, 0xe0, 0x6e, 0x4a, 0x9e, 0xce, 0x64, 0x43, 0x57, 0x65, 0x4a,
    0x9e, 0x32, 0xa6, 0xed, 0x1e, 0xde, 0x61, 0x6a, 0x95, 0x2f, 0x73, 0xab,
    0xc5, 0x33, 0x6d, 0xa1, 0xed, 0x94, 0x60, 0xfb, 0xb8, 0x58, 0x1e, 0x91,
    0x38, 0x54, 0x21, 0x92, 0xee, 0x37, 0xb4, 0x50, 0xfc, 0x29, 0xb1, 0xe4,
    0xdb, 0xcf, 0xa6, 0x1e, 0xf1, 0x8a, 0x41, 0x09, 0xe9, 0x73, 0xca, 0xa1,
    0x85, 0xe2, 0x56, 0x96, 0xae, 0xfb, 0xd9, 0x72, 0xb5, 0x43, 0x62, 0x3b,
    0xfa, 0x1f, 0xca, 0xdf, 0xa7, 0x96, 0x7b, 0xca, 0x8f, 0x7b, 0x12, 0x11,
    0x71, 0xb7, 0xe4, 0x4f, 0x03, 0xa6, 0x8f, 0x73, 0x59, 0xde, 0xfc, 0xe8,
    0xbc, 0xaf, 0xa0, 0xec, 0x40, 0xa1, 0xa5, 0x4a, 0x07, 0xcc, 0xfd, 0x5b,
    0xfe, 0x7c, 0x6d, 0xae, 0x19, 0x5b, 0x68, 0x3b, 0x25, 0x78, 0x36, 0x28,
    0x96, 0xd7, 0xf1, 0xbf, 0x51, 0x71, 0x7e, 0xf5, 0x0f, 0x13, 0x92, 0xfd,
    0x1e, 0xb8, 0xe9, 0xc3, 0x40, 0x1f, 0x91, 0x46, 0xc1, 0xa1, 0x1f, 0x29,
    0xec, 0x7e, 0x30, 0x8f, 0xf8, 0x26, 0x2f, 0x2f, 0xb2, 0x9c, 0x61, 0xef,
    0x0f, 0x21, 0x69, 0x6f, 0x40, 0x53, 0x1c, 0xb3, 0xaf, 0x55, 0x66, 0xcb,
    0x8d, 0x47, 0xc4, 0x79, 0x72, 0xae, 0x1a, 0xb0, 0x56, 0x4a, 0x8f, 0xb7,
    0x7b, 0xd2, 0x8c, 0x37, 0x7a, 0xc6, 0xf2, 0xe6, 0xcb, 0x5d, 0x44, 0x54,
    0xf5, 0x5f, 0x23, 0xf0, 0xc7, 0x2c, 0x92, 0x72, 0x42, 0xef, 0x39, 0x44,
    0x2b, 0x5a, 0x59, 0xbe, 0xbc, 0xe8, 0xd3, 0x08, 0x09, 0xa2, 0x5f, 0x03,
    0x1d, 0x11, 0xc0, 0x27, 0x93, 0x95, 0x12, 0x1c, 0x37, 0xd7, 0xe9, 0x32,
    0x99, 0x01, 0xe0, 0x77, 0x97, 0x85, 0xb5, 0x53, 0x82, 0xd9, 0xa7, 0xdc,
    0x6a, 0x32, 0x6e, 0xb4, 0x45, 0xb6, 0xff, 0xe0, 0x63, 0x1f, 0x0e, 0xfc,
    0x09, 0x58, 0x5a, 0x51, 0xef, 0xb8, 0xd6, 0xca, 0x2a, 0x59, 0x3b, 0x26,
    0x9e, 0x64, 0xeb, 0x05, 0xa3, 0x43, 0x94, 0x47, 0x2e, 0x04, 0x31, 0x23,
    0x2d, 0x77, 0xe2, 0x49, 0xaa, 0xde, 0xe6, 0x73, 0x8c, 0x35, 0xbe, 0xcb,
    0xd8, 0xe2, 0x07, 0x0e, 0x68, 0x35, 0xd9, 0x76, 0x47, 0x74, 0x36, 0xe8,
    0x2e, 0xfa, 0xd0, 0xb0, 0x25, 0xed, 0xb8, 0xf6, 0x04, 0x91, 0xee, 0xa2,
    0x0f, 0x8f, 0x0d, 0x75, 0x59, 0xfd, 0x54, 0x6a, 0x47, 0x40, 0xe7, 0x1c,
    0x88, 0x65, 0xa4, 0x37, 0xcb, 0x2a, 0x97, 0x46, 0x20, 0xc7, 0x4c, 0xa6,
    0xd7, 0x9e, 0x4e, 0x0c, 0x5b, 0xf3, 0x22, 0x70, 0xf6, 0x8e, 0x0f, 0xff,
    0xfa, 0x07, 0x80, 0xfd, 0x6e, 0x04, 0x97, 0xd6, 0x80, 0xa5, 0xbf, 0x7d,
    0x23, 0xaa, 0x53, 0xa5, 0x03, 0xec, 0xbf, 0x90, 0xd1, 0x4b, 0xff, 0xed,
    0x31, 0x6d, 0x96, 0xbc, 0x99, 0x68, 0xc6, 0xf5, 0x40, 0xce, 0xfe, 0x23,
    0x41, 0x1f, 0x0e, 0xdc, 0xe4, 0x63, 0x50, 0x55, 0xfd, 0xcf, 0x44, 0x65,
    0xed, 0xb0, 0xbc, 0x9c, 0xad, 0x17, 0x62, 0x3e, 0x70, 0xd5, 0x45, 0xc6,
    0xd0, 0xd5, 0xd2, 0x72, 0x2d, 0x2f, 0x67, 0xd7, 0x1f, 0x7d, 0x9e, 0xb1,
    0xb5, 0x8f, 0x1c, 0x30, 0xad, 0x65, 0xdb, 0x1f, 0xdf, 0x74, 0x60, 0xd7,
    0x7f, 0xfa, 0xd0, 0x71, 0x95, 0xa8, 0x7b, 0x7c, 0x9a, 0x96, 0x56, 0x88,
    0x3e, 0xb9, 0x37, 0x45, 0x1d, 0x57, 0xf9, 0x33, 0x2f, 0xc9, 0x15, 0x24,
    0xbd, 0x5d, 0x8b, 0x36, 0x8c, 0xf9, 0x7f, 0xaf, 0x93, 0xdc, 0x33, 0xf0,
    0x71, 0x8b, 0x54, 0x4c, 0x32, 0x44, 0x92, 0x59, 0x82, 0xd5, 0x0d, 0x22,
    0xbb, 0x9f, 0x08, 0xe3, 0xfc, 0xbb, 0x61, 0xb8, 0x45, 0x84, 0x3e, 0xf5,
    0xb1, 0x2b, 0x02, 0x12, 0xfd, 0xf6, 0x85, 0xc4, 0x95, 0x12, 0x44, 0x18,
    0xcd, 0xae, 0xc7, 0xdd, 0xca, 0x1d, 0xab, 0x3d, 0xcd, 0x98, 0x36, 0x53,
    0xde, 0x74, 0xc6, 0xab, 0x25, 0xb2, 0xe3, 0x29, 0x81, 0xfd, 0x0f, 0x95,
    0xc7, 0x9c, 0xd3, 0x2b, 0xf2, 0x76, 0xd0, 0x82, 0x74, 0x4c, 0x5b, 0xec,
    0x77, 0x24, 0xda, 0xc7, 0xa7, 0x25, 0xe5, 0x52, 0x9e, 0x58, 0xb9, 0x7d,
    0x5c, 0xdd, 0xbb, 0x80, 0x99, 0x31, 0x73, 0x3e, 0xf9, 0x65, 0x6b, 0x9b,
    0xe2, 0x19, 0x9c, 0x44, 0xfb, 0xff, 0x85, 0xf9, 0x58, 0x69, 0xbb, 0xb4,
    0x06, 0xa3, 0x60, 0x97, 0x81, 0x17, 0x3a, 0x7d, 0x18, 0xfd, 0x33, 0x80,
    0xa0, 0x0f, 0xd8, 0x9e, 0x21, 0xa1, 0x37, 0xa1, 0xbe, 0x3c, 0x92, 0x60,
    0xa4, 0xc7, 0x16, 0xb0, 0x8b, 0xbe, 0x54, 0x9d, 0x1f, 0x02, 0xf6, 0xcd,
    0x70, 0x56, 0xbd, 0xf6, 0x43, 0x79, 0x96, 0xf0, 0x6f, 0x91, 0x69, 0x7d,
    0x79, 0x98, 0x36, 0x1d, 0x9b, 0x3e, 0xe3, 0xcf, 0x31, 0x17, 0xcb, 0xb4,
    0x90, 0x61, 0x5a, 0x64, 0x84, 0x2e, 0xaa, 0x43, 0xa1, 0xea, 0x1a, 0x69,
    0xb9, 0x12, 0x4c, 0x9b, 0x8e, 0xcd, 0x07, 0x19, 0x0b, 0xbd, 0xbe, 0x06,
    0xad, 0xca, 0xb3, 0x26, 0x26, 0xbd, 0x03, 0x8b, 0x3f, 0x07, 0xd6, 0x5e,
    0xc9, 0xfd, 0xe0, 0x48, 0x76, 0xb7, 0x2b, 0x62, 0xc5, 0xee, 0xe1, 0x9c,
    0x33, 0xe4, 0x76, 0xb4, 0x00, 0xa6, 0x1d, 0xcf, 0x3d, 0xe3, 0xe6, 0x56,
    0x53, 0xcc, 0xae, 0x76, 0x57, 0xfc, 0x6d, 0x32, 0x6d, 0xe6, 0x3b, 0x76,
    0xa6, 0x71, 0xfe, 0xbb, 0x67, 0x44, 0x7c, 0x3d, 0x22, 0x9e, 0x01, 0xd3,
    0xdf, 0x76, 0xe6, 0x6e, 0x85, 0x84, 0xeb, 0x85, 0x30, 0x6d, 0x48, 0x96,
    0x69, 0x8b, 0xcb, 0x1e, 0xc8, 0x31, 0xa6, 0xd2, 0xac, 0x84, 0x9a, 0xb7,
    0xba, 0x2b, 0xef, 0x91, 0xe2, 0x3c, 0x7a, 0x99, 0x87, 0x31, 0x26, 0xe9,
    0xf9, 0xdb, 0x85, 0xfd, 0x6e, 0x04, 0xf8, 0x6b, 0x5d, 0xce, 0x19, 0xe2,
    0x30, 0x42, 0x7d, 0xc9, 0xc3, 0x48, 0xaf, 0xfe, 0xb7, 0x0f, 0x66, 0x2f,
    0xf0, 0xc2, 0xa7, 0xe2, 0x7a, 0xda, 0x79, 0x1f, 0x8c, 0xf9, 0x66, 0xe2,
    0x33, 0x14, 0xd3, 0x86, 0x5e, 0x07, 0x2c, 0x7b, 0x99, 0x70, 0x1c, 0xd0,
    0xc3, 0x18, 0x6b, 0x3e, 0xf8, 0x63, 0xb6, 0xf8, 0x73, 0x80, 0x7b, 0x87,
    0xaf, 0xe7, 0x8f, 0x59, 0x60, 0xbe, 0x16, 0xc8, 0x79, 0x7a, 0xad, 0xf7,
    0x4b, 0xf1, 0x8a, 0x93, 0x7e, 0xfd, 0x59, 0x88, 0x69, 0x25, 0xef, 0xc3,
    0x8b, 0x40, 0x3c, 0xa2, 0x94, 0x69, 0x47, 0x80, 0x1f, 0x28, 0x93, 0x5b,
    0xc6, 0x01, 0xe0, 0x8c, 0xc8, 0xda, 0xf5, 0x8d, 0xcc, 0x03, 0xec, 0xf2,
    0x08, 0xf0, 0x62, 0xf6, 0xae, 0x4e, 0x84, 0x3f, 0x2b, 0x70, 0xb3, 0x66,
    0xb4, 0xe5, 0x96, 0x9b, 0x3c, 0xa7, 0xab, 0x13, 0xd7, 0xeb, 0x7f, 0x27,
    0xbf, 0xe3, 0x7d, 0x9b, 0xd9, 0x03, 0x47, 0x9a, 0x5d, 0xd6, 0xca, 0x00,
    0xa4, 0x1e, 0x04, 0xf8, 0xc1, 0xef, 0xba, 0xed, 0x3f, 0x61, 0x2c, 0xf4,
    0x5e, 0x6a, 0x97, 0xcd, 0xce, 0xfb, 0x60, 0x5c, 0xe0, 0x4f, 0xa3, 0x25,
    0xeb, 0xb9, 0x88, 0xc8, 0x7b, 0x9f, 0xc8, 0x1d, 0x37, 0xcb, 0xda, 0x01,
    0x99, 0xec, 0x41, 0xe6, 0x84, 0x52, 0x8d, 0x33, 0x51, 0xe9, 0x71, 0x7c,
    0x92, 0xbf, 0xbd, 0x8b, 0xf8, 0xac, 0x80, 0xd2, 0xd3, 0x7b, 0x8f, 0x0d,
    0x75, 0x79, 0x27, 0x6e, 0x12, 0xcb, 0xfc, 0x00, 0x7c, 0xbf, 0x40, 0x96,
    0xe7, 0x1f, 0xfe, 0xd4, 0x07, 0xac, 0xe7, 0x66, 0x58, 0xc0, 0x01, 0x3c,
    0x99, 0x90, 0xf5, 0xcb, 0x9c, 0x45, 0x45, 0xec, 0x97, 0x8e, 0x3a, 0xcd,
    0x5a, 0x7e, 0xc7, 0xca, 0xd1, 0x7e, 0x33, 0xb1, 0x51, 0x50, 0x9e, 0x54,
    0x29, 0xd3, 0x8a, 0x63, 0x5a, 0x5b, 0x5e, 0x79, 0xb1, 0x57, 0x18, 0xdb,
    0xf8, 0x0f, 0x7e, 0x05, 0x49, 0xe6, 0x73, 0xd9, 0x15, 0xe0, 0x85, 0xb6,
    0xdb, 0x74, 0xb4, 0x8d, 0x88, 0x9d, 0xf7, 0xe1, 0xe4, 0x5d, 0x89, 0x7e,
    0xd3, 0xed, 0x7d, 0x4a, 0x31, 0x6d, 0xbc, 0x52, 0x7d, 0x4c, 0xab, 0xd9,
    0x22, 0x32, 0x2e, 0xf0, 0x7a, 0x4b, 0x65, 0x05, 0x72, 0x61, 0x64, 0x99,
    0x4f, 0x8f, 0xd9, 0x13, 0xb9, 0x1d, 0xb7, 0x2c, 0xb9, 0x64, 0x55, 0xee,
    0x57, 0x36, 0x23, 0x32, 0x71, 0xf8, 0x67, 0x16, 0x59, 0xbf, 0xcc, 0x59,
    0x14, 0x30, 0x6d, 0x26, 0x5a, 0x2b, 0xa1, 0xe8, 0x91, 0x68, 0x4e, 0x39,
    0x28, 0x7f, 0xaa, 0x4c, 0x2b, 0xb6, 0x4b, 0x99, 0x5c, 0xef, 0x73, 0x7c,
    0xba, 0x48, 0x2a, 0xcf, 0xa9, 0x64, 0xc5, 0x88, 0x7e, 0x9d, 0xad, 0x17,
    0x8c, 0x0e, 0x04, 0xee, 0x16, 0x17, 0x0a, 0xf9, 0x3e, 0x1b, 0x91, 0x94,
    0x2b, 0xf7, 0x8a, 0x90, 0x6e, 0x81, 0xa8, 0xe6, 0x73, 0xe0, 0xc0, 0x4d,
    0x69, 0xbd, 0x4d, 0x7a, 0x60, 0xfa, 0x84, 0x03, 0xed, 0xd6, 0x28, 0x42,
    0xef, 0xf1, 0x79, 0xd8, 0xcc, 0x7a, 0xf1, 0x2a, 0x07, 0x76, 0x5f, 0xf1,
    0x61, 0xf3, 0xbe, 0x78, 0xc5, 0xc9, 0x62, 0x5a, 0x0e, 0x40, 0x87, 0x7e,
    0x1e, 0x85, 0x30, 0xdf, 0xef, 0xbe, 0x0c, 0x4b, 0xfa, 0x64, 0xde, 0x12,
    0x54, 0xd7, 0x0f, 0x62, 0x3e, 0x04, 0x7e, 0xad, 0xd0, 0xb1, 0x72, 0xc8,
    0xd9, 0x7d, 0xc5, 0x87, 0x89, 0x59, 0xa2, 0xd8, 0xc2, 0x6a, 0xde, 0x27,
    0x35, 0x85, 0x30, 0xad, 0x78, 0x05, 0x51, 0x2e, 0xd7, 0xc3, 0xe4, 0xf3,
    0x9c, 0xf9, 0x62, 0x73, 0xc3, 0x4b, 0x8c, 0x49, 0xe5, 0xdb, 0x83, 0x8f,
    0x81, 0xee, 0xf1, 0xe9, 0x82, 0x56, 0x96, 0xd6, 0xa1, 0x29, 0x7a, 0x1c,
    0xcf, 0x5e, 0x69, 0x9d, 0x9a, 0xec, 0x57, 0x84, 0x5c, 0xc4, 0xe7, 0x5d,
    0xd9, 0x4d, 0xe0, 0xf1, 0x64, 0xb6, 0xbe, 0x26, 0xbd, 0x03, 0xa1, 0xd7,
    0xd7, 0x30, 0xf6, 0x3e, 0x63, 0x93, 0x2f, 0xf1, 0x31, 0x7d, 0xec, 0x15,
    0xc6, 0xc2, 0xbf, 0x3d, 0xcc, 0xba, 0x2a, 0xe7, 0x25, 0xb3, 0x0c, 0x27,
    0xef, 0xfa, 0xa0, 0xfb, 0x3d, 0x7f, 0xf6, 0x21, 0xeb, 0xfe, 0xae, 0x22,
    0xb5, 0x2b, 0xb3, 0xf6, 0x8d, 0xa9, 0xcf, 0xaf, 0x8d, 0xab, 0xcf, 0x1c,
    0x90, 0xca, 0x6f, 0x7a, 0xa1, 0xed, 0x36, 0x59, 0xef, 0x2b, 0x7f, 0x76,
    0x5f, 0x48, 0x7e, 0x50, 0x0e, 0xbb, 0xee, 0xf3, 0xbb, 0xff, 0xa4, 0x7c,
    0x35, 0x79, 0x5a, 0x35, 0x5f, 0xc6, 0x49, 0x47, 0xe7, 0x50, 0xfe, 0xf1,
    0xc9, 0x6c, 0x47, 0x24, 0xff, 0x65, 0x76, 0x6b, 0x1f, 0xd1, 0xc6, 0x56,
    0xfe, 0xdd, 0x39, 0x6d, 0xf1, 0xf5, 0x2a, 0xaf, 0x4a, 0x67, 0x8b, 0xa4,
    0x3e, 0xc3, 0x4a, 0x24, 0x5f, 0x5f, 0xe9, 0x99, 0x0b, 0x4a, 0x10, 0x99,
    0x72, 0x64, 0x93, 0xac, 0x7d, 0x24, 0xea, 0x57, 0xf4, 0x85, 0x19, 0x17,
    0xf1, 0xb1, 0x08, 0x9f, 0x3d, 0x50, 0x8e, 0x74, 0x4e, 0xdd, 0x66, 0x4c,
    0xed, 0xf7, 0x10, 0xb4, 0x1a, 0x07, 0xa2, 0x27, 0x94, 0x9f, 0x1a, 0x3a,
    0xda, 0x46, 0x54, 0xc8, 0x1b, 0x11, 0xb9, 0x30, 0x79, 0xaa, 0xec, 0x85,
    0x36, 0xf1, 0xa9, 0xb8, 0x76, 0x6b, 0x14, 0xcd, 0x07, 0x7f, 0x2c, 0xe8,
    0x95, 0xfe, 0xfd, 0x08, 0x6b, 0xa5, 0x03, 0x8d, 0xef, 0x16, 0x76, 0xaa,
    0x6c, 0xf4, 0xe1, 0x1c, 0x35, 0x7d, 0x26, 0xff, 0x16, 0xb3, 0xd4, 0x29,
    0xb7, 0xe4, 0x33, 0xfc, 0xc7, 0xf1, 0x3a, 0xd9, 0x76, 0xd6, 0x4a, 0xfe,
    0xd4, 0x94, 0x29, 0xce, 0x9f, 0x06, 0xc3, 0x4c, 0x14, 0x81, 0xf9, 0x1a,
    0xc4, 0x37, 0x03, 0x88, 0xe4, 0x78, 0x93, 0xc4, 0x5a, 0x19, 0x40, 0xe0,
    0xd7, 0x36, 0x21, 0x34, 0xd0, 0x6c, 0x11, 0x9d, 0x5d, 0x06, 0xdc, 0x37,
    0xa5, 0xeb, 0x6b, 0x35, 0x3e, 0x2c, 0x7e, 0xe0, 0x80, 0xf7, 0x39, 0xe5,
    0xf6, 0x4f, 0xcc, 0x12, 0xf5, 0x0e, 0xe6, 0x97, 0x97, 0xe5, 0xf9, 0x8b,
    0x09, 0xf5, 0x8c, 0xd4, 0x12, 0x98, 0x56, 0x97, 0xa3, 0x75, 0xe7, 0xce,
    0xfb, 0x16, 0xf3, 0xe4, 0x68, 0x15, 0x40, 0xcb, 0xbd, 0xa9, 0x1d, 0x63,
    0xda, 0xcc, 0x27, 0x4b, 0x99, 0x79, 0x58, 0x29, 0xe6, 0x49, 0x9d, 0x72,
    0x2a, 0x2e, 0x4f, 0x4a, 0x0b, 0xd2, 0x0c, 0x64, 0xea, 0xc9, 0xcd, 0xe0,
    0xfd, 0x0f, 0xf9, 0x1c, 0xed, 0x4e, 0xd8, 0xdf, 0xff, 0x50, 0x3a, 0x2f,
    0x9b, 0xeb, 0x74, 0x97, 0xb5, 0xaf, 0xf0, 0x0f, 0x63, 0xe7, 0x5a, 0x31,
    0xd0, 0x76, 0x9b, 0x36, 0xb6, 0x88, 0x24, 0x1b, 0x3a, 0x67, 0xb7, 0x2b,
    0x2a, 0x7d, 0x93, 0xe1, 0x02, 0xff, 0x06, 0x82, 0x92, 0xa2, 0xf6, 0xeb,
    0x89, 0xce, 0xa1, 0xc2, 0xbe, 0x66, 0xb8, 0xb1, 0xc5, 0xf7, 0x17, 0x5a,
    0x58, 0x2d, 0x0a, 0xe5, 0x06, 0x36, 0xdf, 0xf5, 0x9d, 0xfc, 0x0d, 0x89,
    0x4c, 0xbd, 0xd4, 0xb4, 0xeb, 0x7f, 0x38, 0x47, 0xed, 0x2b, 0xbc, 0x23,
    0x59, 0xfb, 0xb6, 0x97, 0x72, 0x99, 0x2f, 0xb0, 0x5b, 0xfb, 0xf8, 0x07,
    0x1d, 0xed, 0x2b, 0xfc, 0xaf, 0x16, 0xe5, 0x72, 0xbc, 0xfe, 0x87, 0x73,
    0x64, 0xed, 0xdb, 0x76, 0xb0, 0x36, 0x5e, 0x3e, 0xf7, 0x90, 0x6f, 0xb7,
    0x13, 0xf6, 0x76, 0xdd, 0xe7, 0xe5, 0x27, 0x7f, 0xe3, 0xa3, 0xf2, 0xea,
    0xb0, 0xf0, 0xdb, 0x1c, 0xb2, 0xb1, 0x4d, 0xae, 0x18, 0x43, 0x12, 0x03,
    0xfc, 0xf7, 0x67, 0x73, 0x95, 0xce, 0x09, 0x75, 0xb1, 0x6c, 0xa1, 0x31,
    0x61, 0x09, 0xff, 0xb1, 0x31, 0x27, 0x53, 0x15, 0xf2, 0x5b, 0x0b, 0xc9,
    0x2f, 0x7f, 0x67, 0x16, 0xd3, 0x40, 0x48, 0x95, 0x1c, 0x25, 0x8f, 0x36,
    0x4b, 0xf8, 0xfd, 0xc4, 0xbc, 0x15, 0x86, 0x67, 0x55, 0x32, 0xee, 0x0e,
    0x60, 0xa1, 0x21, 0x41, 0x09, 0xbf, 0x1f, 0xa8, 0xa8, 0xe2, 0x62, 0x22,
    0x15, 0xbb, 0xa8, 0x65, 0x5e, 0x35, 0xa8, 0xbd, 0xc0, 0xc7, 0x60, 0xcf,
    0xc2, 0xc0, 0x94, 0xf0, 0xd9, 0x45, 0xc5, 0x15, 0x89, 0xb6, 0x83, 0xe2,
    0xa7, 0xc4, 0xb0, 0xd6, 0xfb, 0x44, 0x94, 0x28, 0xbd, 0x2d, 0x5b, 0xc2,
    0xfc, 0xa8, 0xba, 0x41, 0x68, 0x61, 0x95, 0xac, 0x7d, 0x3b, 0xc7, 0xb0,
    0xc5, 0xa4, 0x47, 0x4a, 0xf8, 0xfd, 0xc4, 0x82, 0x1b, 0x12, 0xf1, 0x79,
    0xc8, 0xca, 0x7b, 0xea, 0x99, 0xb5, 0xf2, 0xea, 0x30, 0xff, 0x0b, 0xe5,
    0x25, 0x66, 0x2d, 0x61, 0x01, 0xb8, 0x23, 0x82, 0x92, 0xf9, 0xc0, 0xe9,
    0x15, 0x7e, 0xd7, 0x9f, 0xcc, 0xaf, 0x25, 0x91, 0x7b, 0xc8, 0x27, 0xe8,
    0x37, 0xb6, 0x4a, 0xbf, 0x81, 0x5b, 0xc2, 0xe2, 0xf1, 0xff, 0x01, 0xe7,
    0xc1, 0x25, 0xe6, 0xc2, 0x97, 0x00, 0x7b, 0x00, 0x00, 0x00, 0x00, 0x49,
    0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};


/*
 *  Constructs a ConfigurationPanel as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
ConfigurationPanel::ConfigurationPanel( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl ),
      image0( (const char **) image0_data )
{
    QImage img;
    img.loadFromData( image1_data, sizeof( image1_data ), "PNG" );
    image1 = img;
    if ( !name )
	setName( "ConfigurationPanel" );
    setSizeGripEnabled( TRUE );
    ConfigurationPanelLayout = new QGridLayout( this, 1, 1, 11, 6, "ConfigurationPanelLayout"); 

    layout19 = new QVBoxLayout( 0, 0, 6, "layout19"); 

    line1 = new QFrame( this, "line1" );
    line1->setFrameShape( QFrame::HLine );
    line1->setFrameShadow( QFrame::Sunken );
    line1->setFrameShape( QFrame::HLine );
    layout19->addWidget( line1 );

    layout28 = new QHBoxLayout( 0, 0, 6, "layout28"); 

    buttonHelp = new QPushButton( this, "buttonHelp" );
    buttonHelp->setAutoDefault( TRUE );
    layout28->addWidget( buttonHelp );
    Horizontal_Spacing2 = new QSpacerItem( 160, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout28->addItem( Horizontal_Spacing2 );

    buttonSave = new QPushButton( this, "buttonSave" );
    buttonSave->setAutoDefault( TRUE );
    buttonSave->setDefault( TRUE );
    layout28->addWidget( buttonSave );

    buttonOk = new QPushButton( this, "buttonOk" );
    buttonOk->setAutoDefault( TRUE );
    layout28->addWidget( buttonOk );

    buttonCancel = new QPushButton( this, "buttonCancel" );
    buttonCancel->setAutoDefault( TRUE );
    layout28->addWidget( buttonCancel );
    layout19->addLayout( layout28 );

    ConfigurationPanelLayout->addMultiCellLayout( layout19, 1, 1, 0, 1 );

    Menu = new QListBox( this, "Menu" );
    Menu->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)7, 0, 0, Menu->sizePolicy().hasHeightForWidth() ) );
    Menu->setCursor( QCursor( 13 ) );
    Menu->setSelectionMode( QListBox::Single );

    ConfigurationPanelLayout->addWidget( Menu, 0, 0 );

    layout19_2 = new QVBoxLayout( 0, 0, 6, "layout19_2"); 

    TitleTab = new QLabel( this, "TitleTab" );
    QFont TitleTab_font(  TitleTab->font() );
    TitleTab_font.setBold( TRUE );
    TitleTab->setFont( TitleTab_font ); 
    layout19_2->addWidget( TitleTab );

    line2 = new QFrame( this, "line2" );
    line2->setFrameShape( QFrame::HLine );
    line2->setFrameShadow( QFrame::Sunken );
    line2->setFrameShape( QFrame::HLine );
    layout19_2->addWidget( line2 );

    Tab_Signalisations = new QTabWidget( this, "Tab_Signalisations" );

    SIPPage = new QWidget( Tab_Signalisations, "SIPPage" );

    QWidget* privateLayoutWidget = new QWidget( SIPPage, "layout24" );
    privateLayoutWidget->setGeometry( QRect( 16, 12, 401, 393 ) );
    layout24 = new QVBoxLayout( privateLayoutWidget, 11, 6, "layout24"); 

    groupBox1 = new QGroupBox( privateLayoutWidget, "groupBox1" );
    groupBox1->setColumnLayout(0, Qt::Vertical );
    groupBox1->layout()->setSpacing( 6 );
    groupBox1->layout()->setMargin( 11 );
    groupBox1Layout = new QGridLayout( groupBox1->layout() );
    groupBox1Layout->setAlignment( Qt::AlignTop );

    textLabel2 = new QLabel( groupBox1, "textLabel2" );

    groupBox1Layout->addWidget( textLabel2, 0, 0 );

    fullName = new QLineEdit( groupBox1, "fullName" );

    groupBox1Layout->addWidget( fullName, 1, 0 );

    userPart = new QLineEdit( groupBox1, "userPart" );

    groupBox1Layout->addWidget( userPart, 3, 0 );

    textLabel3 = new QLabel( groupBox1, "textLabel3" );

    groupBox1Layout->addWidget( textLabel3, 2, 0 );

    textLabel2_3 = new QLabel( groupBox1, "textLabel2_3" );

    groupBox1Layout->addWidget( textLabel2_3, 4, 0 );

    username = new QLineEdit( groupBox1, "username" );

    groupBox1Layout->addWidget( username, 5, 0 );

    hostPart = new QLineEdit( groupBox1, "hostPart" );

    groupBox1Layout->addWidget( hostPart, 9, 0 );

    sipproxy = new QLineEdit( groupBox1, "sipproxy" );

    groupBox1Layout->addWidget( sipproxy, 11, 0 );

    textLabel3_2_2 = new QLabel( groupBox1, "textLabel3_2_2" );

    groupBox1Layout->addWidget( textLabel3_2_2, 10, 0 );

    password = new QLineEdit( groupBox1, "password" );
    password->setEchoMode( QLineEdit::Password );

    groupBox1Layout->addWidget( password, 7, 0 );

    textLabel1_3 = new QLabel( groupBox1, "textLabel1_3" );

    groupBox1Layout->addWidget( textLabel1_3, 6, 0 );

    textLabel3_2 = new QLabel( groupBox1, "textLabel3_2" );

    groupBox1Layout->addWidget( textLabel3_2, 8, 0 );
    layout24->addWidget( groupBox1 );

    layout23 = new QVBoxLayout( 0, 0, 6, "layout23"); 

    layout19_3 = new QHBoxLayout( 0, 0, 6, "layout19_3"); 

    autoregister = new QCheckBox( privateLayoutWidget, "autoregister" );
    autoregister->setChecked( TRUE );
    layout19_3->addWidget( autoregister );
    spacer7 = new QSpacerItem( 201, 21, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout19_3->addItem( spacer7 );

    Register = new QPushButton( privateLayoutWidget, "Register" );
    layout19_3->addWidget( Register );
    layout23->addLayout( layout19_3 );
    spacer9 = new QSpacerItem( 20, 21, QSizePolicy::Minimum, QSizePolicy::Expanding );
    layout23->addItem( spacer9 );
    layout24->addLayout( layout23 );
    Tab_Signalisations->insertTab( SIPPage, QString("") );

    STUNPage = new QWidget( Tab_Signalisations, "STUNPage" );

    groupBox3 = new QGroupBox( STUNPage, "groupBox3" );
    groupBox3->setGeometry( QRect( 10, 110, 253, 100 ) );

    textLabel1_5 = new QLabel( groupBox3, "textLabel1_5" );
    textLabel1_5->setGeometry( QRect( 10, 38, 229, 16 ) );

    STUNserver = new QLineEdit( groupBox3, "STUNserver" );
    STUNserver->setGeometry( QRect( 10, 60, 229, 23 ) );

    stunButtonGroup = new QButtonGroup( STUNPage, "stunButtonGroup" );
    stunButtonGroup->setGeometry( QRect( 10, 10, 90, 81 ) );
    stunButtonGroup->setColumnLayout(0, Qt::Vertical );
    stunButtonGroup->layout()->setSpacing( 6 );
    stunButtonGroup->layout()->setMargin( 11 );
    stunButtonGroupLayout = new QVBoxLayout( stunButtonGroup->layout() );
    stunButtonGroupLayout->setAlignment( Qt::AlignTop );

    useStunNo = new QRadioButton( stunButtonGroup, "useStunNo" );
    useStunNo->setChecked( TRUE );
    stunButtonGroupLayout->addWidget( useStunNo );

    useStunYes = new QRadioButton( stunButtonGroup, "useStunYes" );
    useStunYes->setChecked( FALSE );
    stunButtonGroupLayout->addWidget( useStunYes );
    Tab_Signalisations->insertTab( STUNPage, QString("") );

    DTMFPage = new QWidget( Tab_Signalisations, "DTMFPage" );

    SettingsDTMF = new QGroupBox( DTMFPage, "SettingsDTMF" );
    SettingsDTMF->setGeometry( QRect( 10, 10, 301, 130 ) );
    SettingsDTMF->setColumnLayout(0, Qt::Vertical );
    SettingsDTMF->layout()->setSpacing( 6 );
    SettingsDTMF->layout()->setMargin( 11 );
    SettingsDTMFLayout = new QGridLayout( SettingsDTMF->layout() );
    SettingsDTMFLayout->setAlignment( Qt::AlignTop );

    layout11 = new QVBoxLayout( 0, 0, 6, "layout11"); 

    layout10 = new QHBoxLayout( 0, 0, 6, "layout10"); 

    playTones = new QCheckBox( SettingsDTMF, "playTones" );
    playTones->setChecked( TRUE );
    layout10->addWidget( playTones );
    spacer6 = new QSpacerItem( 111, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout10->addItem( spacer6 );
    layout11->addLayout( layout10 );

    layout7 = new QHBoxLayout( 0, 0, 6, "layout7"); 

    labelPulseLength = new QLabel( SettingsDTMF, "labelPulseLength" );
    layout7->addWidget( labelPulseLength );
    spacer3 = new QSpacerItem( 115, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout7->addItem( spacer3 );

    pulseLength = new QSpinBox( SettingsDTMF, "pulseLength" );
    pulseLength->setMaxValue( 1500 );
    pulseLength->setMinValue( 10 );
    pulseLength->setValue( 250 );
    layout7->addWidget( pulseLength );
    layout11->addLayout( layout7 );

    layout8 = new QHBoxLayout( 0, 0, 6, "layout8"); 

    labelSendDTMF = new QLabel( SettingsDTMF, "labelSendDTMF" );
    layout8->addWidget( labelSendDTMF );
    spacer4 = new QSpacerItem( 85, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout8->addItem( spacer4 );

    sendDTMFas = new QComboBox( FALSE, SettingsDTMF, "sendDTMFas" );
    layout8->addWidget( sendDTMFas );
    layout11->addLayout( layout8 );

    SettingsDTMFLayout->addLayout( layout11, 0, 0 );
    Tab_Signalisations->insertTab( DTMFPage, QString("") );
    layout19_2->addWidget( Tab_Signalisations );

    Tab_Audio = new QTabWidget( this, "Tab_Audio" );

    DriversPage = new QWidget( Tab_Audio, "DriversPage" );

    DriverChoice = new QButtonGroup( DriversPage, "DriverChoice" );
    DriverChoice->setGeometry( QRect( 10, 10, 104, 81 ) );
    DriverChoice->setColumnLayout(0, Qt::Vertical );
    DriverChoice->layout()->setSpacing( 6 );
    DriverChoice->layout()->setMargin( 11 );
    DriverChoiceLayout = new QVBoxLayout( DriverChoice->layout() );
    DriverChoiceLayout->setAlignment( Qt::AlignTop );

    ossButton = new QRadioButton( DriverChoice, "ossButton" );
    ossButton->setChecked( TRUE );
    DriverChoiceLayout->addWidget( ossButton );

    alsaButton = new QRadioButton( DriverChoice, "alsaButton" );
    alsaButton->setEnabled( TRUE );
    DriverChoiceLayout->addWidget( alsaButton );
    Tab_Audio->insertTab( DriversPage, QString("") );

    CodecsPage = new QWidget( Tab_Audio, "CodecsPage" );

    CodecsChoice = new QButtonGroup( CodecsPage, "CodecsChoice" );
    CodecsChoice->setGeometry( QRect( 20, 10, 133, 157 ) );

    QWidget* privateLayoutWidget_2 = new QWidget( CodecsChoice, "layout18" );
    privateLayoutWidget_2->setGeometry( QRect( 10, 20, 110, 120 ) );
    layout18 = new QGridLayout( privateLayoutWidget_2, 1, 1, 11, 6, "layout18"); 

    layout17 = new QVBoxLayout( 0, 0, 6, "layout17"); 

    codec1 = new QComboBox( FALSE, privateLayoutWidget_2, "codec1" );
    layout17->addWidget( codec1 );

    codec2 = new QComboBox( FALSE, privateLayoutWidget_2, "codec2" );
    layout17->addWidget( codec2 );

    codec3 = new QComboBox( FALSE, privateLayoutWidget_2, "codec3" );
    layout17->addWidget( codec3 );

    layout18->addLayout( layout17, 0, 0 );

    layout18_2 = new QVBoxLayout( 0, 0, 6, "layout18_2"); 

    textLabel1_4 = new QLabel( privateLayoutWidget_2, "textLabel1_4" );
    layout18_2->addWidget( textLabel1_4 );

    textLabel1_4_2 = new QLabel( privateLayoutWidget_2, "textLabel1_4_2" );
    layout18_2->addWidget( textLabel1_4_2 );

    textLabel1_4_3 = new QLabel( privateLayoutWidget_2, "textLabel1_4_3" );
    layout18_2->addWidget( textLabel1_4_3 );

    layout18->addLayout( layout18_2, 0, 1 );
    Tab_Audio->insertTab( CodecsPage, QString("") );

    RingPage = new QWidget( Tab_Audio, "RingPage" );

    ringsChoice = new QComboBox( FALSE, RingPage, "ringsChoice" );
    ringsChoice->setGeometry( QRect( 20, 21, 150, 30 ) );
    Tab_Audio->insertTab( RingPage, QString("") );
    layout19_2->addWidget( Tab_Audio );

    Tab_Preferences = new QTabWidget( this, "Tab_Preferences" );

    DriversPage_2 = new QWidget( Tab_Preferences, "DriversPage_2" );

    SkinChoice = new QComboBox( FALSE, DriversPage_2, "SkinChoice" );
    SkinChoice->setGeometry( QRect( 12, 42, 110, 27 ) );

    buttonApplySkin = new QPushButton( DriversPage_2, "buttonApplySkin" );
    buttonApplySkin->setGeometry( QRect( 136, 40, 80, 32 ) );
    Tab_Preferences->insertTab( DriversPage_2, QString("") );

    TabPage = new QWidget( Tab_Preferences, "TabPage" );

    QWidget* privateLayoutWidget_3 = new QWidget( TabPage, "layout17" );
    privateLayoutWidget_3->setGeometry( QRect( 10, 10, 262, 200 ) );
    layout17_2 = new QVBoxLayout( privateLayoutWidget_3, 11, 6, "layout17_2"); 

    layout16 = new QHBoxLayout( 0, 0, 6, "layout16"); 

    textLabel1_2 = new QLabel( privateLayoutWidget_3, "textLabel1_2" );
    layout16->addWidget( textLabel1_2 );

    zoneToneChoice = new QComboBox( FALSE, privateLayoutWidget_3, "zoneToneChoice" );
    layout16->addWidget( zoneToneChoice );
    spacer5 = new QSpacerItem( 31, 21, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout16->addItem( spacer5 );
    layout17_2->addLayout( layout16 );

    confirmationToQuit = new QCheckBox( privateLayoutWidget_3, "confirmationToQuit" );
    confirmationToQuit->setChecked( TRUE );
    layout17_2->addWidget( confirmationToQuit );

    checkedTray = new QCheckBox( privateLayoutWidget_3, "checkedTray" );
    layout17_2->addWidget( checkedTray );

    layout16_2 = new QHBoxLayout( 0, 0, 6, "layout16_2"); 

    textLabel1_6 = new QLabel( privateLayoutWidget_3, "textLabel1_6" );
    layout16_2->addWidget( textLabel1_6 );

    voicemailNumber = new QLineEdit( privateLayoutWidget_3, "voicemailNumber" );
    layout16_2->addWidget( voicemailNumber );
    spacer6_2 = new QSpacerItem( 61, 21, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout16_2->addItem( spacer6_2 );
    layout17_2->addLayout( layout16_2 );
    Tab_Preferences->insertTab( TabPage, QString("") );
    layout19_2->addWidget( Tab_Preferences );

    Tab_About = new QTabWidget( this, "Tab_About" );

    DriversPage_3 = new QWidget( Tab_About, "DriversPage_3" );

    pixmapLabel1 = new QLabel( DriversPage_3, "pixmapLabel1" );
    pixmapLabel1->setGeometry( QRect( 50, 40, 312, 91 ) );
    pixmapLabel1->setPixmap( image0 );
    pixmapLabel1->setScaledContents( TRUE );

    textLabel2_2 = new QLabel( DriversPage_3, "textLabel2_2" );
    textLabel2_2->setGeometry( QRect( 20, 170, 371, 121 ) );
    Tab_About->insertTab( DriversPage_3, QString("") );

    CodecsPage_2 = new QWidget( Tab_About, "CodecsPage_2" );

    textLabel1 = new QLabel( CodecsPage_2, "textLabel1" );
    textLabel1->setGeometry( QRect( 85, 126, 291, 131 ) );

    pixmapLabel2 = new QLabel( CodecsPage_2, "pixmapLabel2" );
    pixmapLabel2->setGeometry( QRect( 130, 50, 173, 48 ) );
    pixmapLabel2->setPixmap( image1 );
    pixmapLabel2->setScaledContents( TRUE );
    Tab_About->insertTab( CodecsPage_2, QString("") );
    layout19_2->addWidget( Tab_About );

    ConfigurationPanelLayout->addLayout( layout19_2, 0, 1 );
    languageChange();
    resize( QSize(561, 552).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( Menu, SIGNAL( clicked(QListBoxItem*) ), Tab_Signalisations, SLOT( setFocus() ) );
    connect( buttonCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );
    connect( Menu, SIGNAL( clicked(QListBoxItem*) ), this, SLOT( changeTabSlot() ) );
    connect( buttonSave, SIGNAL( clicked() ), this, SLOT( saveSlot() ) );
    connect( stunButtonGroup, SIGNAL( clicked(int) ), this, SLOT( useStunSlot(int) ) );
    connect( buttonApplySkin, SIGNAL( clicked() ), this, SLOT( applySkinSlot() ) );
    connect( DriverChoice, SIGNAL( clicked(int) ), this, SLOT( driverSlot(int) ) );
    connect( buttonOk, SIGNAL( clicked() ), this, SLOT( accept() ) );

    // tab order
    setTabOrder( Tab_Signalisations, fullName );
    setTabOrder( fullName, username );
    setTabOrder( username, password );
    setTabOrder( password, hostPart );
    setTabOrder( hostPart, sipproxy );
    setTabOrder( sipproxy, autoregister );
    setTabOrder( autoregister, buttonSave );
    setTabOrder( buttonSave, buttonOk );
    setTabOrder( buttonOk, buttonCancel );
    setTabOrder( buttonCancel, userPart );
    setTabOrder( userPart, buttonHelp );
    setTabOrder( buttonHelp, SkinChoice );
    setTabOrder( SkinChoice, zoneToneChoice );
    setTabOrder( zoneToneChoice, confirmationToQuit );
    setTabOrder( confirmationToQuit, checkedTray );
    setTabOrder( checkedTray, voicemailNumber );
    setTabOrder( voicemailNumber, useStunYes );
    setTabOrder( useStunYes, STUNserver );
    setTabOrder( STUNserver, playTones );
    setTabOrder( playTones, pulseLength );
    setTabOrder( pulseLength, sendDTMFas );
    setTabOrder( sendDTMFas, Menu );
    setTabOrder( Menu, Tab_Audio );
    setTabOrder( Tab_Audio, ossButton );
    setTabOrder( ossButton, codec1 );
    setTabOrder( codec1, codec2 );
    setTabOrder( codec2, codec3 );
    setTabOrder( codec3, Tab_Preferences );
    setTabOrder( Tab_Preferences, Tab_About );
    setTabOrder( Tab_About, Register );
    setTabOrder( Register, useStunNo );
    setTabOrder( useStunNo, ringsChoice );
    setTabOrder( ringsChoice, buttonApplySkin );
    init();
}

/*
 *  Destroys the object and frees any allocated resources
 */
ConfigurationPanel::~ConfigurationPanel()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void ConfigurationPanel::languageChange()
{
    setCaption( tr( "Configuration panel" ) );
    buttonHelp->setText( tr( "&Help" ) );
    buttonHelp->setAccel( QKeySequence( tr( "F1" ) ) );
    buttonSave->setText( tr( "&Save" ) );
    buttonSave->setAccel( QKeySequence( tr( "Alt+S" ) ) );
    buttonOk->setText( tr( "&OK" ) );
    buttonOk->setAccel( QKeySequence( tr( "Alt+O" ) ) );
    buttonCancel->setText( tr( "&Cancel" ) );
    buttonCancel->setAccel( QKeySequence( tr( "F, Backspace" ) ) );
    Menu->setCurrentItem( -1 );
    TitleTab->setText( tr( "Setup signalisation" ) );
    groupBox1->setTitle( QString::null );
    textLabel2->setText( tr( "Full name" ) );
    textLabel3->setText( tr( "User Part of SIP URL" ) );
    textLabel2_3->setText( tr( "Authorization user" ) );
    textLabel3_2_2->setText( tr( "SIP proxy" ) );
    textLabel1_3->setText( tr( "Password" ) );
    textLabel3_2->setText( tr( "Host part of SIP URL" ) );
    autoregister->setText( tr( "Auto-register" ) );
    Register->setText( tr( "Register" ) );
    Tab_Signalisations->changeTab( SIPPage, tr( "SIP Authentication" ) );
    groupBox3->setTitle( tr( "Settings " ) );
    textLabel1_5->setText( tr( "STUN server (address:port)" ) );
    stunButtonGroup->setTitle( tr( "Use STUN" ) );
    useStunNo->setText( tr( "No" ) );
    useStunYes->setText( tr( "Yes" ) );
    Tab_Signalisations->changeTab( STUNPage, tr( "STUN" ) );
    SettingsDTMF->setTitle( tr( "Settings" ) );
    playTones->setText( tr( "Play tones locally" ) );
    labelPulseLength->setText( tr( "Pulse length" ) );
    pulseLength->setSuffix( tr( " ms" ) );
    labelSendDTMF->setText( tr( "Send DTMF as" ) );
    sendDTMFas->clear();
    sendDTMFas->insertItem( tr( "SIP INFO" ) );
    sendDTMFas->setCurrentItem( 0 );
    Tab_Signalisations->changeTab( DTMFPage, tr( "DTMF" ) );
    DriverChoice->setTitle( tr( "Drivers" ) );
    ossButton->setText( tr( "OSS" ) );
    alsaButton->setText( tr( "ALSA" ) );
    Tab_Audio->changeTab( DriversPage, tr( "Drivers" ) );
    CodecsChoice->setTitle( tr( "Supported codecs" ) );
    codec1->clear();
    codec1->insertItem( tr( "G711u" ) );
    codec1->insertItem( tr( "G711a" ) );
    codec1->insertItem( tr( "GSM" ) );
    codec2->clear();
    codec2->insertItem( tr( "G711a" ) );
    codec2->insertItem( tr( "G711u" ) );
    codec2->insertItem( tr( "GSM" ) );
    codec3->clear();
    codec3->insertItem( tr( "G711u" ) );
    codec3->insertItem( tr( "G711a" ) );
    codec3->insertItem( tr( "GSM" ) );
    textLabel1_4->setText( tr( "1" ) );
    textLabel1_4_2->setText( tr( "2" ) );
    textLabel1_4_3->setText( tr( "3" ) );
    Tab_Audio->changeTab( CodecsPage, tr( "Codecs" ) );
    Tab_Audio->changeTab( RingPage, tr( "Ringtones" ) );
    buttonApplySkin->setText( tr( "&Apply" ) );
    buttonApplySkin->setAccel( QKeySequence( tr( "Alt+A" ) ) );
    Tab_Preferences->changeTab( DriversPage_2, tr( "Themes" ) );
    textLabel1_2->setText( tr( "Zone tone:" ) );
    zoneToneChoice->clear();
    zoneToneChoice->insertItem( tr( "North America" ) );
    zoneToneChoice->insertItem( tr( "France" ) );
    zoneToneChoice->insertItem( tr( "Australia" ) );
    zoneToneChoice->insertItem( tr( "United Kingdom" ) );
    zoneToneChoice->insertItem( tr( "Spain" ) );
    zoneToneChoice->insertItem( tr( "Italy" ) );
    zoneToneChoice->insertItem( tr( "Japan" ) );
    confirmationToQuit->setText( tr( "Show confirmation to quit." ) );
    checkedTray->setText( tr( "Minimize to tray" ) );
    textLabel1_6->setText( tr( "Voicemail:" ) );
    Tab_Preferences->changeTab( TabPage, tr( "Options" ) );
    textLabel2_2->setText( tr( "<p align=\"center\">\n"
"Copyright (C) 2004-2005 Savoir-faire Linux inc.<br><br>\n"
"Laurielle LEA &lt;laurielle.lea@savoirfairelinux.com&gt;<br><br>\n"
"SFLPhone-0.3.1 is released under the General Public License.<br>\n"
"For more information, see http://www.sflphone.org<br>\n"
"</p>" ) );
    Tab_About->changeTab( DriversPage_3, tr( "About SFLPhone" ) );
    textLabel1->setText( tr( "<p align=\"center\">Website: http://www.savoirfairelinux.com<br><br>\n"
"5505, Saint-Laurent - bureau 3030<br>\n"
"Montreal, Quebec H2T 1S6</p>" ) );
    Tab_About->changeTab( CodecsPage_2, tr( "About Savoir-faire Linux inc." ) );
}

