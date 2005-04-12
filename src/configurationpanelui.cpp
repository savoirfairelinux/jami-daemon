/****************************************************************************
** Form implementation generated from reading ui file 'configurationpanel.ui'
**
** Created: Tue Apr 12 09:35:08 2005
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "configurationpanelui.h"

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
    0x08, 0x06, 0x00, 0x00, 0x00, 0x63, 0x57, 0xfa, 0xde, 0x00, 0x00, 0x0d,
    0x4d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0xed, 0x5d, 0x5f, 0x68, 0x1c,
    0xc7, 0x1d, 0xfe, 0x1d, 0x38, 0x20, 0x17, 0x51, 0xce, 0xa5, 0x0f, 0x96,
    0x09, 0x45, 0x0a, 0x4d, 0x40, 0x2e, 0x84, 0xac, 0xaa, 0x3c, 0x9c, 0xda,
    0x04, 0xaf, 0xc8, 0x4b, 0xcf, 0x72, 0x70, 0x4e, 0xc9, 0x83, 0xa5, 0x36,
    0x0f, 0x77, 0x96, 0x8b, 0xa5, 0xf6, 0x25, 0x72, 0xf2, 0x22, 0xdb, 0x41,
    0xb9, 0xf8, 0x41, 0x91, 0x2f, 0xe0, 0x56, 0x97, 0xa2, 0x54, 0x67, 0x30,
    0x95, 0x12, 0x10, 0x92, 0x1f, 0xe4, 0xb3, 0x0a, 0xa6, 0x3a, 0x17, 0x85,
    0x53, 0xc0, 0xae, 0x4e, 0x45, 0x45, 0x32, 0x38, 0x20, 0x15, 0xb5, 0x48,
    0x41, 0x14, 0x59, 0x08, 0x21, 0x1d, 0xc2, 0xf0, 0xf5, 0x61, 0x6f, 0xf6,
    0xf6, 0xdf, 0xec, 0xce, 0xec, 0x9d, 0x1c, 0x39, 0xbe, 0x79, 0xf9, 0x6c,
    0xed, 0xfc, 0xf9, 0xcd, 0xec, 0x6f, 0xbe, 0xfd, 0xe6, 0xb7, 0x33, 0x7b,
    0x04, 0x22, 0x1a, 0x22, 0xa2, 0xb2, 0x21, 0x00, 0xac, 0x00, 0x00, 0x90,
    0x2d, 0x20, 0xf6, 0x80, 0xb2, 0xb7, 0x53, 0xc1, 0x67, 0x16, 0x4b, 0x76,
    0xd0, 0xf4, 0xc3, 0x55, 0x00, 0x80, 0x32, 0x38, 0x05, 0xba, 0x02, 0x50,
    0xef, 0x18, 0x17, 0xd5, 0x11, 0xa0, 0xfb, 0xce, 0x02, 0x00, 0xe0, 0xbb,
    0xee, 0x78, 0x05, 0x9f, 0x5e, 0xf4, 0x55, 0x70, 0x61, 0x5d, 0x23, 0xd0,
    0x60, 0x22, 0x0d, 0x02, 0x40, 0x57, 0xc6, 0xa4, 0x31, 0x08, 0x20, 0x3a,
    0x81, 0x8a, 0x03, 0x57, 0x50, 0x1a, 0xe5, 0x0a, 0x00, 0x50, 0x6f, 0x78,
    0x33, 0xaa, 0x2c, 0xaa, 0x23, 0xc0, 0xea, 0xd6, 0x4e, 0x45, 0x42, 0x54,
    0x50, 0x08, 0x85, 0x32, 0x62, 0x4f, 0x63, 0x44, 0x3f, 0x8c, 0x2a, 0x83,
    0xf1, 0xe9, 0x85, 0x8a, 0xe3, 0x56, 0xd0, 0x13, 0x3d, 0x33, 0x2c, 0xac,
    0xef, 0x40, 0x49, 0x95, 0x97, 0x59, 0xdd, 0x50, 0x19, 0x9c, 0xaa, 0x48,
    0x86, 0x0a, 0xba, 0xa2, 0x6b, 0x06, 0xac, 0x03, 0xc1, 0x27, 0xc0, 0xb0,
    0x56, 0x0c, 0x26, 0xd2, 0x15, 0xc7, 0xad, 0x20, 0x17, 0xb9, 0x17, 0x16,
    0xd6, 0x77, 0x10, 0x4c, 0xf8, 0x60, 0xcc, 0x64, 0x1a, 0x94, 0x02, 0x68,
    0x70, 0x0a, 0x94, 0x02, 0xc2, 0x37, 0x67, 0x01, 0x00, 0x63, 0x8b, 0x5a,
    0x94, 0x41, 0xb4, 0x9e, 0x60, 0xdf, 0x58, 0xe9, 0x8e, 0x0b, 0x60, 0x60,
    0x70, 0x08, 0xd1, 0xf3, 0x9d, 0x88, 0x76, 0x74, 0x16, 0x30, 0x8a, 0xe8,
    0xf9, 0x4e, 0xf4, 0x27, 0xfa, 0x01, 0x00, 0xab, 0xab, 0xab, 0x4f, 0x8f,
    0x24, 0x01, 0x90, 0x9e, 0x28, 0xef, 0x84, 0xde, 0xd9, 0xda, 0x41, 0x7f,
    0x02, 0x88, 0x76, 0x74, 0x22, 0x74, 0x42, 0x45, 0xb4, 0x23, 0xea, 0xbf,
    0xfe, 0x7d, 0xb0, 0xcf, 0x09, 0xcb, 0xc3, 0xb0, 0x7d, 0x9a, 0xa1, 0xd8,
    0x82, 0x6b, 0xca, 0x2e, 0xe1, 0x89, 0x30, 0x2e, 0x76, 0xb4, 0xf6, 0xea,
    0x1b, 0x15, 0xa0, 0x51, 0x43, 0xa5, 0x31, 0xc4, 0xfd, 0x7f, 0x4f, 0x6f,
    0xfc, 0xa9, 0x60, 0xf6, 0x9e, 0xde, 0xb8, 0x6e, 0x7f, 0xd9, 0xec, 0xdd,
    0xb0, 0x8f, 0x47, 0xf7, 0xfb, 0xdd, 0xbe, 0x26, 0xf2, 0xbe, 0xd8, 0xe7,
    0x80, 0xf6, 0x99, 0xb7, 0x07, 0x69, 0x86, 0x15, 0x49, 0xd1, 0xc9, 0x59,
    0x5f, 0x1a, 0xd7, 0x0f, 0xe3, 0x46, 0xcf, 0x77, 0x9a, 0x6e, 0x44, 0xb8,
    0x25, 0x82, 0x81, 0xc1, 0x21, 0x00, 0xc0, 0xf0, 0xc8, 0x18, 0x90, 0x00,
    0xd4, 0x96, 0xb0, 0x7e, 0x5d, 0x69, 0x0c, 0x61, 0x36, 0x37, 0x7b, 0xf0,
    0x19, 0x97, 0xd9, 0x5f, 0x2e, 0x87, 0x80, 0xd9, 0x61, 0x95, 0xc6, 0x10,
    0xa2, 0x1d, 0x9d, 0x25, 0x31, 0x6d, 0x59, 0xed, 0xe3, 0xa0, 0xed, 0x0f,
    0xaa, 0x0c, 0xc3, 0x16, 0xd0, 0x2b, 0x85, 0x46, 0xb3, 0x25, 0x69, 0x5c,
    0xf5, 0x46, 0x56, 0xdc, 0xa1, 0x2c, 0x0c, 0x8b, 0x3d, 0xfe, 0x00, 0xe2,
    0xa1, 0x66, 0x5f, 0x7a, 0x32, 0xed, 0x5a, 0x3f, 0xcb, 0x57, 0xaa, 0x83,
    0x94, 0x74, 0xdd, 0xcd, 0xbe, 0x55, 0x1f, 0xf6, 0x01, 0x08, 0x9d, 0x50,
    0x6d, 0x4f, 0x20, 0xb7, 0x7a, 0xf0, 0x10, 0xbe, 0x26, 0xb6, 0xac, 0x7d,
    0x5e, 0xf9, 0x03, 0x43, 0x44, 0x14, 0x23, 0xa2, 0x14, 0x11, 0xc5, 0x00,
    0x04, 0x3e, 0x1a, 0x27, 0x3a, 0xd4, 0x4a, 0xf4, 0x58, 0x0c, 0xa3, 0x4a,
    0x8e, 0x52, 0xbf, 0x6a, 0x20, 0x5e, 0x8a, 0xfd, 0x35, 0x47, 0xd7, 0xe7,
    0x1a, 0x84, 0xeb, 0xe3, 0xe1, 0xf0, 0xa9, 0x35, 0x6a, 0x7f, 0xf9, 0x58,
    0x20, 0x65, 0xb4, 0xd7, 0x01, 0x73, 0xb9, 0x59, 0x9c, 0x3d, 0xf7, 0x3b,
    0xca, 0x53, 0x9e, 0xaa, 0xa8, 0x8a, 0x42, 0x6a, 0x13, 0x25, 0xfb, 0x13,
    0x9e, 0xe5, 0x4c, 0x08, 0xa0, 0xb5, 0xad, 0x9d, 0x96, 0xbf, 0x59, 0xd6,
    0xeb, 0x31, 0x62, 0x58, 0x0d, 0x53, 0x57, 0x47, 0x8c, 0xea, 0x5e, 0x7a,
    0x21, 0xd0, 0x74, 0x42, 0x45, 0x7e, 0x3b, 0x4f, 0x55, 0xd5, 0x55, 0x34,
    0x73, 0x37, 0x43, 0xa9, 0x40, 0x20, 0x60, 0xac, 0x2f, 0x33, 0x9d, 0xc5,
    0xc5, 0xcb, 0x17, 0x69, 0x73, 0x7b, 0x93, 0x94, 0xfa, 0x7a, 0x6a, 0xfe,
    0xcb, 0x17, 0x01, 0x66, 0x67, 0xc3, 0x97, 0x0d, 0xd4, 0x74, 0xbf, 0x99,
    0xf2, 0xdb, 0x79, 0x53, 0xfd, 0x4a, 0x7d, 0x3d, 0x29, 0xaf, 0x28, 0x74,
    0xe1, 0xbd, 0x0b, 0xe6, 0xfa, 0x12, 0xc0, 0xf1, 0x2f, 0x1b, 0xa8, 0x8a,
    0xaa, 0xe8, 0xea, 0xa7, 0x57, 0xa9, 0xf9, 0xf5, 0x5f, 0x04, 0x92, 0x83,
    0x43, 0x48, 0xfd, 0x39, 0xa5, 0x97, 0x0f, 0x1e, 0x3d, 0x42, 0x9b, 0xdf,
    0x3e, 0xa2, 0x3c, 0xe5, 0x69, 0xf1, 0x7e, 0xce, 0x66, 0x0f, 0x43, 0x66,
    0xb7, 0x53, 0xff, 0xf4, 0x72, 0x00, 0x9a, 0x4f, 0x9d, 0xd4, 0xeb, 0x33,
    0xe6, 0xab, 0xaa, 0xae, 0xa2, 0x19, 0x35, 0x43, 0x6b, 0x1d, 0x6b, 0x54,
    0x73, 0xac, 0x70, 0x5f, 0x04, 0xec, 0xab, 0xaa, 0xae, 0xd2, 0xfb, 0x6b,
    0x6c, 0x27, 0xf9, 0x79, 0x8a, 0x26, 0x6f, 0x4d, 0xd2, 0xf2, 0xb7, 0xcb,
    0xa6, 0x76, 0x82, 0x47, 0x8f, 0x50, 0xe6, 0xad, 0xdb, 0xb4, 0xfb, 0xce,
    0x2e, 0x1d, 0xfe, 0xe1, 0x0f, 0x02, 0x29, 0xb2, 0xcc, 0x3c, 0x3f, 0x6f,
    0xb8, 0xdc, 0x92, 0xac, 0x86, 0xf5, 0x7a, 0x83, 0x26, 0xb4, 0x87, 0x01,
    0x76, 0x8d, 0x16, 0x6e, 0x89, 0x68, 0xf6, 0x4c, 0x67, 0x85, 0x66, 0x3c,
    0x2b, 0x1f, 0x3a, 0xa1, 0x6a, 0x1d, 0x29, 0x30, 0x6d, 0xf7, 0xfb, 0xdd,
    0x3a, 0x23, 0x85, 0xde, 0x50, 0x81, 0x1d, 0x00, 0x6f, 0x14, 0xdb, 0x0b,
    0xb7, 0x84, 0xed, 0xf6, 0x9d, 0xb6, 0x5f, 0xef, 0x4f, 0x0c, 0xd8, 0xed,
    0xdb, 0x00, 0xa2, 0xe7, 0x3b, 0x4d, 0xf5, 0x59, 0x99, 0xcf, 0x91, 0x11,
    0x1d, 0x34, 0x29, 0xfb, 0x7f, 0x4f, 0x6f, 0x9c, 0x3f, 0x5e, 0xe0, 0x68,
    0xfe, 0x3d, 0x68, 0xfd, 0x32, 0xb4, 0xc7, 0xc6, 0x6f, 0x36, 0x37, 0xab,
    0x8f, 0x83, 0x9e, 0xff, 0x74, 0x89, 0xf6, 0x59, 0x19, 0xff, 0x74, 0x71,
    0x8d, 0x11, 0x39, 0xd3, 0x66, 0x1e, 0xef, 0x02, 0xd3, 0xeb, 0x4c, 0x1b,
    0x9b, 0x98, 0xc5, 0xf5, 0x7f, 0xf9, 0x60, 0xc4, 0x1f, 0x67, 0x08, 0xe7,
    0x9a, 0x1d, 0x59, 0xb6, 0x8b, 0x88, 0x3e, 0x93, 0x64, 0x6e, 0x37, 0x8c,
    0x5f, 0x6e, 0xa5, 0x1a, 0x0e, 0x73, 0x58, 0x99, 0xf2, 0xf8, 0xab, 0x0d,
    0x36, 0x06, 0xb1, 0x32, 0x45, 0x7d, 0x7d, 0x3d, 0x35, 0xff, 0xb2, 0x99,
    0xda, 0xdb, 0x5a, 0xcd, 0x8c, 0x04, 0x20, 0x37, 0x97, 0xa3, 0x86, 0x86,
    0x9f, 0xdb, 0x18, 0xfa, 0x78, 0xa3, 0x02, 0x23, 0x23, 0x25, 0x3f, 0x4f,
    0x91, 0x91, 0x49, 0x6e, 0x4f, 0x8c, 0x17, 0x99, 0xc7, 0x62, 0xc7, 0xe2,
    0xfd, 0x1c, 0xdd, 0xbe, 0x75, 0x9b, 0x2e, 0x7d, 0x74, 0x85, 0xf2, 0x94,
    0xa7, 0x60, 0x75, 0x90, 0xba, 0x3a, 0xba, 0xa8, 0xbd, 0xfd, 0xed, 0x80,
    0x9b, 0xfd, 0x8c, 0x91, 0x92, 0x8d, 0x21, 0xb0, 0x76, 0x62, 0x1d, 0x31,
    0xea, 0xfa, 0xed, 0xd9, 0x40, 0xac, 0xa3, 0x13, 0x73, 0x73, 0x73, 0x36,
    0xa6, 0x6e, 0x7b, 0xa7, 0x8d, 0x4e, 0xbe, 0xd9, 0x12, 0x18, 0x19, 0x19,
    0x43, 0xfb, 0x3f, 0x5b, 0x29, 0xb6, 0x19, 0x23, 0x3a, 0x54, 0x45, 0x0d,
    0xaf, 0x34, 0x50, 0xd7, 0xb9, 0x98, 0xce, 0x70, 0xc6, 0x76, 0x22, 0xa7,
    0x22, 0x54, 0xf7, 0xe1, 0x25, 0xef, 0x71, 0x28, 0x97, 0x7d, 0xa7, 0x4e,
    0x52, 0xec, 0x5c, 0x17, 0xb1, 0xeb, 0x61, 0x35, 0x4c, 0xf5, 0xfd, 0x57,
    0x4c, 0xf7, 0xf7, 0x62, 0x6f, 0x1c, 0x93, 0xb7, 0x26, 0x29, 0x4f, 0x79,
    0x0a, 0xbd, 0x1a, 0xa2, 0xa6, 0x3f, 0x7d, 0x16, 0xd0, 0x34, 0xc4, 0x5e,
    0x69, 0xf1, 0x58, 0xb6, 0x17, 0xc1, 0x96, 0xd6, 0xcb, 0xc7, 0xb4, 0xd2,
    0xd1, 0x04, 0x00, 0xfd, 0x89, 0x01, 0x60, 0x45, 0x30, 0x8a, 0xe0, 0x52,
    0x2f, 0x63, 0x98, 0x70, 0x4b, 0xc4, 0x54, 0x1e, 0x39, 0xe8, 0x4c, 0xe1,
    0xa4, 0x09, 0x7b, 0x7a, 0xe3, 0x76, 0x46, 0x3a, 0x5d, 0xb4, 0x67, 0x78,
    0x74, 0xcc, 0x93, 0x09, 0x95, 0xc6, 0x90, 0xbe, 0x9a, 0x77, 0xb4, 0x77,
    0xc3, 0xa2, 0xe1, 0x2d, 0xfd, 0x70, 0xea, 0x2f, 0x6b, 0x17, 0x39, 0xd8,
    0xfb, 0xe3, 0x32, 0xae, 0xac, 0xbf, 0xe1, 0x96, 0xb0, 0xa9, 0x3e, 0xb6,
    0x88, 0x95, 0xb5, 0x0f, 0x0f, 0xcd, 0xed, 0x47, 0xce, 0xb4, 0x99, 0x18,
    0x1d, 0xd3, 0xb0, 0xdd, 0x3f, 0x00, 0xd0, 0x98, 0x16, 0xf2, 0x5a, 0xd6,
    0x88, 0xa1, 0xcb, 0xad, 0x34, 0xe3, 0xc8, 0xb5, 0x44, 0x57, 0xbf, 0x5e,
    0xa6, 0xf7, 0xef, 0xd6, 0x95, 0x85, 0x69, 0x35, 0x6d, 0xbb, 0x4c, 0xed,
    0x2f, 0xbf, 0x20, 0xa7, 0x51, 0x89, 0x68, 0x6d, 0x75, 0x15, 0x35, 0x35,
    0x35, 0x94, 0xf9, 0x6a, 0x86, 0x9a, 0xbf, 0x69, 0xa2, 0xd6, 0xbf, 0xd9,
    0x35, 0xab, 0x51, 0x63, 0x75, 0x7d, 0x70, 0x81, 0xe6, 0xee, 0xe7, 0x68,
    0x73, 0x7b, 0x93, 0xcb, 0xd8, 0x2c, 0xff, 0xa3, 0x04, 0x70, 0xbd, 0xa0,
    0xe5, 0x98, 0xb6, 0x5d, 0xfe, 0x66, 0x99, 0xda, 0xcf, 0xc5, 0xf4, 0xf2,
    0xf1, 0xcb, 0x3d, 0x74, 0xf2, 0xcd, 0x96, 0x80, 0x95, 0xa9, 0x79, 0x9a,
    0x33, 0x33, 0x9d, 0xc5, 0x85, 0xf7, 0x2e, 0xe8, 0x8c, 0x1c, 0xfb, 0xfb,
    0x5d, 0x0e, 0x93, 0x45, 0x31, 0x37, 0xf7, 0x40, 0xb7, 0xab, 0xeb, 0x1f,
    0x5f, 0xdb, 0x98, 0x6a, 0xed, 0xdb, 0x35, 0xa2, 0xc7, 0x44, 0x74, 0x88,
    0xa8, 0xaa, 0xfa, 0x30, 0x25, 0x3f, 0xb9, 0xea, 0xc8, 0xb4, 0x36, 0x7b,
    0x24, 0xc7, 0x41, 0xda, 0x3e, 0x81, 0x27, 0xa2, 0x11, 0x83, 0xd5, 0x41,
    0x9a, 0x9a, 0xb8, 0xad, 0x79, 0xbc, 0x32, 0x58, 0x3a, 0x23, 0xba, 0xa5,
    0xfe, 0x99, 0xf2, 0x31, 0xae, 0x30, 0xd3, 0x0a, 0x60, 0x7a, 0x32, 0x6d,
    0x62, 0x8c, 0xa1, 0x42, 0x58, 0xcc, 0xa6, 0x69, 0x0b, 0xab, 0xd9, 0xe1,
    0xd1, 0x31, 0x47, 0x86, 0xc1, 0x2a, 0xf8, 0x0c, 0xce, 0x34, 0x21, 0x6b,
    0xd7, 0xa1, 0xbc, 0x97, 0xe6, 0x64, 0xe5, 0x85, 0x34, 0xa3, 0x44, 0xff,
    0x67, 0x73, 0xb3, 0x8e, 0xfd, 0xb1, 0x32, 0x24, 0xd3, 0xb4, 0xc6, 0x71,
    0x70, 0x62, 0x68, 0x59, 0xfb, 0xac, 0xed, 0x8b, 0xc6, 0xcb, 0x03, 0xab,
    0x5b, 0x3b, 0x38, 0xf6, 0xc7, 0xc3, 0x25, 0x33, 0xe0, 0xd8, 0x5b, 0x6b,
    0xd4, 0x5a, 0x5f, 0xc3, 0xe1, 0x5b, 0xa2, 0xf1, 0x07, 0x6b, 0xf4, 0xf6,
    0xad, 0x1a, 0xa2, 0x7c, 0xe9, 0x8c, 0x8b, 0xcb, 0xad, 0x5c, 0x86, 0x32,
    0x69, 0x31, 0xa5, 0xc1, 0x33, 0x9f, 0x71, 0xa6, 0x47, 0xcf, 0xe4, 0xc8,
    0xca, 0x98, 0xd6, 0xf2, 0xcd, 0x2d, 0x61, 0x38, 0xad, 0xce, 0x8d, 0xda,
    0xcb, 0xc6, 0x44, 0xe7, 0x73, 0x94, 0x8a, 0x69, 0xf9, 0x8c, 0x1a, 0xcf,
    0x49, 0xc3, 0x31, 0x6c, 0x3d, 0xd3, 0x06, 0xf6, 0x24, 0x58, 0x3c, 0x95,
    0xa3, 0xd4, 0x87, 0x62, 0x9a, 0xd1, 0xca, 0xb4, 0x32, 0xda, 0xdf, 0xf8,
    0xa4, 0x61, 0x7f, 0x77, 0x1c, 0x07, 0x4e, 0x39, 0x69, 0xfb, 0xbc, 0x98,
    0x9e, 0x2c, 0x4f, 0xca, 0xc2, 0x5a, 0x81, 0x50, 0x0e, 0x06, 0x4c, 0x4e,
    0xb9, 0x32, 0xad, 0x9e, 0x0a, 0x6f, 0xcc, 0x4a, 0x6d, 0x6f, 0x78, 0x1e,
    0xc2, 0xab, 0x62, 0xec, 0x15, 0x18, 0x12, 0x00, 0x7b, 0x53, 0xc6, 0xa2,
    0x08, 0x26, 0x06, 0x38, 0x0d, 0x60, 0xda, 0x45, 0xe3, 0xc1, 0xb2, 0x6a,
    0x76, 0xb8, 0xee, 0xa4, 0x99, 0xf5, 0x28, 0x83, 0x8b, 0x7d, 0xa6, 0xf8,
    0x27, 0x1c, 0x56, 0xcd, 0xab, 0x45, 0x26, 0xf3, 0xd4, 0x8c, 0x12, 0x4c,
    0xcb, 0xd3, 0xb4, 0xc6, 0x27, 0x50, 0xe8, 0x0d, 0xd5, 0xcc, 0xc0, 0x16,
    0xfb, 0xa4, 0x35, 0xad, 0xcb, 0xfd, 0x62, 0x4f, 0x36, 0xdd, 0xbe, 0xc2,
    0xfd, 0x62, 0x6b, 0x06, 0x3d, 0x7a, 0x10, 0xf5, 0x1b, 0x35, 0x60, 0x18,
    0xbc, 0x4d, 0xe8, 0x3c, 0xc9, 0x65, 0x58, 0x5e, 0x0a, 0x7c, 0x9e, 0x21,
    0xfa, 0x5f, 0xb3, 0xaf, 0x76, 0xbb, 0x2f, 0xb7, 0x52, 0x3d, 0x67, 0x46,
    0xee, 0x6e, 0xed, 0xe0, 0x6c, 0xd7, 0x59, 0x9a, 0x7b, 0xf0, 0x40, 0x58,
    0x2b, 0x29, 0xf5, 0xf5, 0xf4, 0xf1, 0xa5, 0x8f, 0xa9, 0xee, 0xc5, 0x3a,
    0xb2, 0xc6, 0x67, 0x83, 0x47, 0x8f, 0x50, 0x7e, 0x7b, 0xd7, 0x51, 0xd3,
    0xd9, 0x98, 0xa1, 0x17, 0x38, 0x7e, 0xcb, 0xac, 0xd1, 0x18, 0xe3, 0x58,
    0x19, 0x8e, 0x17, 0xff, 0x34, 0x62, 0xdd, 0xd1, 0x3a, 0x1a, 0xf8, 0xf4,
    0x2a, 0xd5, 0xbd, 0xa4, 0x69, 0xf8, 0xd4, 0x09, 0x15, 0xcc, 0x0e, 0x27,
    0xcd, 0x58, 0x77, 0xb4, 0x8e, 0x22, 0xe9, 0x9b, 0x65, 0x61, 0xda, 0x26,
    0xd5, 0x1c, 0x3f, 0x16, 0x19, 0x87, 0x9b, 0x2d, 0x11, 0xb0, 0x38, 0xab,
    0x93, 0x7d, 0x4c, 0x9b, 0x5b, 0xed, 0x30, 0xae, 0x09, 0x78, 0xe3, 0x51,
    0xf7, 0x62, 0x1d, 0x8d, 0x7f, 0x31, 0x42, 0xa9, 0x40, 0x20, 0x50, 0x12,
    0xd3, 0x46, 0x0a, 0x9b, 0x61, 0xfc, 0xa6, 0x9e, 0xcc, 0x82, 0xcf, 0x37,
    0x64, 0xf0, 0x66, 0x14, 0x40, 0xdb, 0x14, 0xc3, 0x89, 0x13, 0x62, 0x85,
    0xb3, 0xe7, 0x00, 0x05, 0x66, 0x66, 0x71, 0xd3, 0x8d, 0x42, 0x7c, 0x16,
    0xc5, 0x55, 0x2d, 0xd3, 0xbe, 0x26, 0x06, 0x35, 0x30, 0x03, 0x2b, 0xd7,
    0x9f, 0xe8, 0x77, 0xb5, 0x33, 0x3d, 0x99, 0x36, 0xe5, 0x2f, 0x96, 0x1b,
    0xb0, 0xdb, 0xc5, 0xf2, 0x1b, 0xec, 0x61, 0xf6, 0xb2, 0x72, 0xd9, 0x69,
    0x89, 0x37, 0x87, 0x86, 0xfe, 0xb2, 0xfe, 0x38, 0xfe, 0xdd, 0x60, 0x17,
    0x1b, 0x2f, 0x16, 0x45, 0xb0, 0x96, 0xf3, 0xb2, 0xcf, 0xf5, 0xcd, 0xa3,
    0x43, 0x7b, 0xd1, 0xf3, 0xda, 0x2b, 0x65, 0x4c, 0x9b, 0xc7, 0x99, 0x7c,
    0xed, 0x7b, 0xbd, 0x56, 0xdc, 0xb5, 0x55, 0x6a, 0xca, 0x2e, 0x6d, 0x80,
    0x92, 0x72, 0xed, 0x07, 0x13, 0x07, 0x7f, 0x73, 0x4b, 0x05, 0xf7, 0x0f,
    0xc9, 0x57, 0x94, 0xc0, 0x63, 0x37, 0x97, 0x74, 0xf2, 0xa1, 0x75, 0x5d,
    0xb5, 0x5a, 0xe1, 0xa4, 0x45, 0xf7, 0x1d, 0x40, 0x45, 0xf1, 0x30, 0x65,
    0xe5, 0x54, 0xf0, 0xf7, 0x03, 0xc5, 0x99, 0xb6, 0x4f, 0x6c, 0x37, 0x57,
    0xa9, 0x49, 0xd4, 0x1e, 0xb7, 0x8e, 0xd5, 0x72, 0x98, 0x5b, 0x49, 0x95,
    0xce, 0xd0, 0x3b, 0x85, 0x09, 0x91, 0x5d, 0xd9, 0x00, 0xc0, 0xa9, 0xcf,
    0x70, 0x3d, 0xbb, 0xb2, 0x51, 0xd6, 0x89, 0xb2, 0xc3, 0x99, 0x78, 0xac,
    0xbd, 0xf8, 0xf4, 0x02, 0xe2, 0x28, 0x9c, 0x92, 0x2e, 0x43, 0x7b, 0x03,
    0xf7, 0x96, 0x10, 0x45, 0x61, 0xd3, 0x12, 0x80, 0x85, 0x75, 0x73, 0x7f,
    0xd2, 0x0f, 0x57, 0xa1, 0x8e, 0x68, 0xd7, 0x35, 0x9c, 0x42, 0x1c, 0xfe,
    0xcf, 0xfc, 0x2d, 0xac, 0x6f, 0x20, 0x0e, 0x73, 0x7d, 0x11, 0x4b, 0x7d,
    0x62, 0x4c, 0x9b, 0x7c, 0x22, 0xfe, 0xaa, 0xa7, 0x52, 0x99, 0xd6, 0x6f,
    0x39, 0x21, 0xb4, 0xd6, 0x07, 0xbb, 0x16, 0xdc, 0x97, 0xf8, 0x32, 0x8a,
    0x7b, 0x43, 0x94, 0x41, 0x98, 0xb5, 0xf4, 0x16, 0x1c, 0xfb, 0x9b, 0x5d,
    0x41, 0x49, 0xfd, 0x1d, 0xb8, 0x67, 0xaf, 0x37, 0x28, 0xd8, 0x5f, 0x42,
    0xe1, 0x09, 0x27, 0xd1, 0xbf, 0x88, 0xcb, 0x6e, 0x40, 0xe3, 0xf8, 0x79,
    0x33, 0xed, 0x13, 0x62, 0x58, 0x6b, 0x2a, 0x85, 0x69, 0xfd, 0x96, 0x13,
    0x7a, 0x34, 0x59, 0xea, 0xb3, 0x1e, 0xc6, 0x8c, 0x8c, 0x66, 0xb9, 0x76,
    0xab, 0x23, 0x00, 0x8f, 0x29, 0x45, 0x18, 0x8f, 0xdb, 0x0f, 0xce, 0x78,
    0xc5, 0x51, 0x5a, 0x7f, 0xe3, 0x9c, 0x7a, 0x8d, 0x0e, 0xb4, 0xb3, 0x07,
    0xa8, 0x23, 0xfc, 0xf1, 0x16, 0x3a, 0xf3, 0x07, 0xa0, 0xf6, 0x5a, 0x9a,
    0x3b, 0x6e, 0xd6, 0xf2, 0x9e, 0x4c, 0xbb, 0x1f, 0x89, 0xed, 0x55, 0x48,
    0x2f, 0xba, 0xe7, 0x3b, 0x88, 0x4c, 0xeb, 0xc5, 0xa0, 0x5e, 0xfb, 0x91,
    0x15, 0xd6, 0x7f, 0xd9, 0xf6, 0x5d, 0xfa, 0x91, 0x5d, 0x71, 0x66, 0xda,
    0xf8, 0xb4, 0x8f, 0x76, 0x0c, 0x18, 0x9f, 0x96, 0x60, 0x70, 0xf0, 0x77,
    0x09, 0xba, 0xee, 0x19, 0x71, 0x29, 0x57, 0x9b, 0x74, 0x2e, 0x47, 0x6e,
    0x1e, 0x1e, 0xf2, 0xe7, 0x93, 0x42, 0xc9, 0x78, 0xa6, 0xac, 0x1f, 0xc0,
    0x86, 0xc3, 0xe2, 0x2e, 0x02, 0x67, 0xbb, 0xbc, 0xb4, 0xe9, 0x77, 0xc9,
    0xb4, 0x22, 0xdf, 0x85, 0x08, 0x26, 0xe4, 0xb5, 0x6e, 0x7c, 0x7a, 0x41,
    0x9a, 0x69, 0xb9, 0x8e, 0x22, 0x33, 0x51, 0x64, 0xea, 0x85, 0xfb, 0x97,
    0x86, 0x6a, 0x93, 0x45, 0xcd, 0xdd, 0x7d, 0x67, 0x01, 0xbc, 0xb5, 0x07,
    0x5d, 0x71, 0xff, 0x12, 0x91, 0xab, 0x26, 0xe1, 0x45, 0x09, 0x22, 0x37,
    0x0b, 0x33, 0xb0, 0xaf, 0x90, 0xff, 0x86, 0x0f, 0xa7, 0x75, 0x6a, 0xb7,
    0x20, 0xf6, 0xf5, 0xc4, 0xd1, 0x6a, 0x91, 0x51, 0x1c, 0x58, 0xa6, 0x75,
    0x1b, 0x4f, 0xa9, 0x37, 0x7b, 0x4e, 0x0e, 0xc4, 0xe9, 0x07, 0xb8, 0x4c,
    0x5b, 0xda, 0x77, 0x24, 0xe2, 0xd3, 0xce, 0x71, 0x74, 0x78, 0x68, 0x65,
    0x1e, 0x43, 0x8b, 0x60, 0x50, 0xa0, 0x7e, 0xdb, 0x0c, 0x66, 0x18, 0xb6,
    0xbc, 0x38, 0xe8, 0xce, 0x2c, 0x80, 0xae, 0xf1, 0x67, 0x9e, 0xb4, 0xd3,
    0xf2, 0x66, 0xa4, 0x45, 0x43, 0x47, 0x6e, 0xda, 0xcf, 0x96, 0xd9, 0x1c,
    0xc5, 0xea, 0x58, 0xdf, 0x29, 0xd3, 0xf2, 0x35, 0xad, 0x13, 0x46, 0x27,
    0xc4, 0xce, 0xa6, 0xf9, 0x61, 0xda, 0x27, 0xa1, 0x69, 0x79, 0x98, 0x5d,
    0xd9, 0x90, 0x3e, 0x6b, 0xa8, 0xa4, 0xc4, 0x34, 0xbf, 0xeb, 0x6a, 0x57,
    0x77, 0xb0, 0xe4, 0x94, 0xeb, 0x0c, 0xf1, 0xd2, 0xa6, 0x8e, 0x4e, 0xeb,
    0x52, 0x9f, 0xad, 0x7d, 0xc9, 0x55, 0xf1, 0x41, 0xd2, 0xb4, 0x0a, 0x80,
    0xb4, 0xf1, 0x8c, 0x19, 0x34, 0x47, 0x37, 0xee, 0x5f, 0x8e, 0x8c, 0x0a,
    0x9c, 0xa8, 0x70, 0xe9, 0xc7, 0x81, 0xd0, 0xb4, 0x0e, 0x28, 0x73, 0xaa,
    0xbb, 0x16, 0x10, 0x8e, 0xa3, 0x73, 0x99, 0xa9, 0xe8, 0xb0, 0x7c, 0xcd,
    0xbb, 0x2f, 0x4c, 0x6b, 0xd1, 0xd4, 0xd6, 0x7c, 0xc1, 0x3e, 0x6f, 0xc7,
    0x3b, 0x48, 0x9a, 0x96, 0xc7, 0x1c, 0x3b, 0x7b, 0x66, 0xcd, 0xee, 0xb5,
    0xca, 0xee, 0xbe, 0xf3, 0x14, 0x68, 0x5a, 0xc1, 0xf2, 0x4e, 0x58, 0x7b,
    0x4d, 0x7c, 0x83, 0x3f, 0x81, 0x88, 0x22, 0xa3, 0xf6, 0x19, 0x05, 0x00,
    0x24, 0x78, 0x66, 0xcc, 0x97, 0xd3, 0x0a, 0xd4, 0xeb, 0x37, 0xde, 0x79,
    0x50, 0x34, 0xad, 0xd0, 0x29, 0x62, 0x87, 0xfa, 0xb0, 0x65, 0x70, 0x74,
    0x00, 0x4e, 0xf1, 0x52, 0x13, 0xa3, 0x1d, 0x30, 0x4d, 0xcb, 0xec, 0x96,
    0x3d, 0x73, 0x28, 0xfa, 0x49, 0xac, 0xd2, 0x4f, 0x2e, 0x3c, 0x3f, 0x43,
    0x78, 0xb7, 0x89, 0x64, 0x93, 0xdf, 0x5d, 0x5e, 0xd9, 0xdf, 0x3c, 0xa2,
    0xa6, 0x9f, 0xfc, 0xc8, 0xf5, 0xe4, 0xc2, 0xd9, 0x2b, 0x00, 0xaf, 0xfc,
    0xea, 0xef, 0x77, 0xa9, 0xa6, 0x70, 0xaa, 0x53, 0xe6, 0xe4, 0x03, 0xc3,
    0x40, 0xef, 0x18, 0x8c, 0xf5, 0xc5, 0x5f, 0x5b, 0xa4, 0x8b, 0xaf, 0xff,
    0x4c, 0xaf, 0xaf, 0xf9, 0x46, 0x16, 0x77, 0xff, 0xdb, 0x44, 0xf4, 0x78,
    0x9c, 0xd4, 0x9f, 0xb6, 0x52, 0x7b, 0xbb, 0xd8, 0x99, 0xb6, 0x23, 0x9f,
    0x8c, 0xd3, 0xe6, 0x63, 0xf1, 0x71, 0x18, 0xea, 0x31, 0xef, 0x6f, 0x75,
    0xba, 0x7f, 0xc2, 0x67, 0xea, 0x38, 0xf8, 0x00, 0x40, 0xc2, 0xa1, 0x5e,
    0xb7, 0xfd, 0xcc, 0xbb, 0x7b, 0xc0, 0xe1, 0xe7, 0x88, 0xfc, 0xfa, 0x53,
    0xf0, 0xd0, 0x38, 0x3d, 0xfa, 0xc0, 0x7d, 0xbf, 0xb4, 0xee, 0xc1, 0xb5,
    0x25, 0x30, 0xdf, 0x93, 0x62, 0x5a, 0xa1, 0xdd, 0x5d, 0x44, 0xee, 0x4c,
    0x2b, 0x30, 0x93, 0xcb, 0xa5, 0x69, 0x45, 0xed, 0xf5, 0xc3, 0x4c, 0xc6,
    0xf2, 0x0b, 0x9c, 0xb3, 0x78, 0x52, 0xed, 0x3b, 0x60, 0x6d, 0xd2, 0xd9,
    0x1e, 0xae, 0x86, 0x5d, 0x01, 0xdc, 0xfc, 0x48, 0x19, 0xd4, 0xee, 0x7d,
    0x7c, 0x5a, 0x0b, 0x67, 0xf1, 0xea, 0x77, 0x7c, 0xe2, 0x18, 0xef, 0x2f,
    0xfb, 0xc7, 0xf0, 0xfc, 0xaa, 0x90, 0xf6, 0xf0, 0x8a, 0x32, 0x08, 0x3b,
    0xad, 0x64, 0x3b, 0x32, 0x5a, 0xca, 0xab, 0x9e, 0x28, 0x3c, 0xf6, 0x0e,
    0xb8, 0x4d, 0x08, 0x09, 0x4d, 0xab, 0x8e, 0xc8, 0x7f, 0x40, 0x43, 0xe4,
    0x8b, 0xea, 0x4e, 0xda, 0x9c, 0x17, 0x6f, 0xf7, 0x2b, 0x11, 0xa2, 0x13,
    0xce, 0x5f, 0x04, 0x72, 0x8c, 0x76, 0x40, 0x8b, 0xab, 0xba, 0x45, 0x05,
    0x78, 0x71, 0xe9, 0x81, 0x7b, 0x4b, 0xdc, 0x28, 0x43, 0x30, 0x01, 0x0c,
    0xdc, 0x5b, 0xb2, 0x95, 0x23, 0xf3, 0x80, 0xfb, 0xfb, 0x12, 0x8c, 0x74,
    0xf2, 0xf1, 0x3d, 0x04, 0x55, 0xc2, 0xc1, 0xfc, 0x3c, 0x31, 0xdc, 0x66,
    0x7c, 0xda, 0x70, 0xb2, 0xa0, 0xec, 0x9a, 0xd6, 0xd1, 0x61, 0xbc, 0xc7,
    0x47, 0x86, 0xa9, 0x55, 0x16, 0x47, 0xf7, 0x58, 0x9d, 0xb3, 0xdd, 0x71,
    0x3c, 0x06, 0x74, 0xd4, 0x9c, 0x2e, 0xf9, 0x45, 0x9f, 0x6c, 0xd8, 0x02,
    0x14, 0x97, 0xfe, 0x32, 0xfb, 0xcd, 0x9a, 0x96, 0xfc, 0x7f, 0x61, 0x86,
    0x69, 0x1c, 0x99, 0x24, 0xfb, 0x3d, 0x84, 0x60, 0x75, 0x2b, 0x2d, 0x76,
    0x88, 0x6b, 0x51, 0x37, 0x4d, 0xeb, 0x17, 0x99, 0x86, 0xf4, 0xd2, 0xb4,
    0xc6, 0xef, 0x47, 0x08, 0x6b, 0x5a, 0x07, 0x1c, 0x99, 0x5f, 0xc2, 0xaf,
    0x6f, 0xf1, 0x4f, 0x31, 0x9b, 0x34, 0xad, 0xe1, 0xfe, 0xd5, 0xfd, 0xe1,
    0x36, 0xfd, 0x7b, 0xfb, 0x24, 0xb7, 0x9c, 0xfa, 0xfc, 0x0c, 0x1d, 0x79,
    0xb7, 0x89, 0x94, 0xaf, 0x16, 0x89, 0x5e, 0x3b, 0x4e, 0xf4, 0xd5, 0x22,
    0x65, 0xfe, 0x73, 0x9c, 0x36, 0xb7, 0x33, 0x34, 0xe7, 0xb2, 0xc6, 0x50,
    0x9f, 0xcf, 0x50, 0xe6, 0xdd, 0x66, 0x5d, 0x6b, 0xee, 0xee, 0x01, 0x97,
    0x9e, 0x23, 0x72, 0xd2, 0xbc, 0xa2, 0xda, 0xd4, 0x8a, 0x33, 0x00, 0xae,
    0x0b, 0xd4, 0xe7, 0xb9, 0x9a, 0x15, 0xc1, 0x9e, 0xcc, 0x82, 0x14, 0xd1,
    0x8a, 0x46, 0x25, 0x18, 0x4a, 0xbd, 0x39, 0xa2, 0x42, 0x78, 0xa8, 0x4c,
    0x4c, 0x6b, 0x5d, 0x85, 0x5b, 0xe3, 0xb0, 0x6e, 0x4c, 0xdb, 0x7d, 0x47,
    0xce, 0x6e, 0x27, 0x8d, 0xe8, 0xc4, 0x40, 0xca, 0xe0, 0x94, 0x6b, 0xbd,
    0xc3, 0xf3, 0x85, 0x71, 0x2e, 0xd3, 0x93, 0x86, 0xb7, 0xd7, 0x80, 0xcf,
    0x8c, 0x02, 0x71, 0x67, 0x1e, 0xc2, 0x5d, 0xdb, 0x03, 0x80, 0x63, 0xc1,
    0x28, 0x24, 0x35, 0x67, 0x9f, 0x76, 0x02, 0x41, 0x24, 0xc9, 0x7e, 0x3d,
    0x51, 0xf4, 0x8d, 0x91, 0x15, 0xad, 0xfb, 0x5e, 0xfd, 0x22, 0x6f, 0x60,
    0xbd, 0xae, 0x97, 0xf5, 0x37, 0x24, 0xac, 0x76, 0x49, 0x94, 0x1b, 0x9e,
    0x5f, 0x32, 0xed, 0x4f, 0xe5, 0x69, 0x5f, 0x65, 0x70, 0x0a, 0xea, 0x88,
    0xf6, 0xa2, 0x43, 0xdf, 0x8f, 0xeb, 0xe2, 0x78, 0xc3, 0xf3, 0xda, 0x3e,
    0xda, 0x60, 0x9f, 0x56, 0x5e, 0xbd, 0x91, 0x45, 0x04, 0x65, 0xda, 0xc7,
    0x0b, 0x4d, 0xcb, 0xaa, 0x23, 0xc5, 0xdf, 0xf8, 0xa8, 0xbd, 0x96, 0xd6,
    0x7f, 0x9b, 0x83, 0xab, 0x6d, 0xdc, 0x34, 0x06, 0x0f, 0xb3, 0x4b, 0xee,
    0x0e, 0x2b, 0xfb, 0xfd, 0x03, 0xbf, 0x9a, 0xb0, 0x82, 0xdf, 0x6f, 0x74,
    0x65, 0x2a, 0x3f, 0xbf, 0xb5, 0xc0, 0x3b, 0xec, 0xa8, 0x8c, 0xc8, 0xbd,
    0x93, 0x17, 0x7a, 0xb5, 0x59, 0xc1, 0x67, 0x12, 0xbd, 0x33, 0xfa, 0x60,
    0xdc, 0x52, 0xd1, 0xaf, 0x24, 0xa8, 0xe0, 0xb3, 0x81, 0x62, 0x19, 0x51,
    0xd4, 0x2e, 0x7e, 0xe2, 0xab, 0xa2, 0x18, 0x4c, 0x68, 0x1a, 0xec, 0x20,
    0x0c, 0x4c, 0x05, 0x0f, 0x2e, 0x8a, 0x17, 0x28, 0x68, 0xcc, 0xfd, 0x62,
    0x58, 0x15, 0xe0, 0xbe, 0x01, 0xa9, 0x60, 0x05, 0x8d, 0x28, 0x5d, 0x20,
    0xbb, 0xb2, 0xe1, 0x7a, 0x26, 0x48, 0x16, 0x4b, 0x0a, 0x8f, 0x54, 0xf0,
    0x99, 0x44, 0xff, 0x15, 0x40, 0x8b, 0x43, 0xd6, 0xfa, 0x60, 0x56, 0x76,
    0xf6, 0xa7, 0xc2, 0xac, 0x15, 0xf4, 0x83, 0xe5, 0xa9, 0x08, 0xc0, 0xf0,
    0xbc, 0x16, 0xd7, 0x8b, 0x8c, 0x66, 0x6d, 0xe7, 0xe0, 0x23, 0x28, 0x6e,
    0x92, 0xa8, 0xfc, 0x06, 0x6e, 0x05, 0x4b, 0xc5, 0xff, 0x03, 0x2c, 0xfa,
    0x16, 0x4c, 0x4d, 0x8a, 0x1e, 0xc3, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
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

    layout8 = new QVBoxLayout( 0, 0, 6, "layout8"); 

    line1 = new QFrame( this, "line1" );
    line1->setFrameShape( QFrame::HLine );
    line1->setFrameShadow( QFrame::Sunken );
    line1->setFrameShape( QFrame::HLine );
    layout8->addWidget( line1 );

    layout7 = new QHBoxLayout( 0, 0, 6, "layout7"); 

    buttonHelp = new QPushButton( this, "buttonHelp" );
    buttonHelp->setAutoDefault( TRUE );
    layout7->addWidget( buttonHelp );
    Horizontal_Spacing2 = new QSpacerItem( 190, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout7->addItem( Horizontal_Spacing2 );

    buttonSave = new QPushButton( this, "buttonSave" );
    buttonSave->setAutoDefault( TRUE );
    buttonSave->setDefault( TRUE );
    layout7->addWidget( buttonSave );

    buttonCancel = new QPushButton( this, "buttonCancel" );
    buttonCancel->setAutoDefault( TRUE );
    layout7->addWidget( buttonCancel );
    layout8->addLayout( layout7 );

    ConfigurationPanelLayout->addMultiCellLayout( layout8, 1, 1, 0, 1 );

    Menu = new QListBox( this, "Menu" );
    Menu->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)7, 0, 0, Menu->sizePolicy().hasHeightForWidth() ) );
    Menu->setCursor( QCursor( 13 ) );
    Menu->setSelectionMode( QListBox::Single );

    ConfigurationPanelLayout->addWidget( Menu, 0, 0 );

    layout17 = new QVBoxLayout( 0, 0, 6, "layout17"); 

    TitleTab = new QLabel( this, "TitleTab" );
    QFont TitleTab_font(  TitleTab->font() );
    TitleTab_font.setBold( TRUE );
    TitleTab->setFont( TitleTab_font ); 
    layout17->addWidget( TitleTab );

    line2 = new QFrame( this, "line2" );
    line2->setFrameShape( QFrame::HLine );
    line2->setFrameShadow( QFrame::Sunken );
    line2->setFrameShape( QFrame::HLine );
    layout17->addWidget( line2 );

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

    layout19 = new QHBoxLayout( 0, 0, 6, "layout19"); 

    autoregister = new QCheckBox( privateLayoutWidget, "autoregister" );
    autoregister->setChecked( TRUE );
    layout19->addWidget( autoregister );
    spacer7 = new QSpacerItem( 201, 21, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout19->addItem( spacer7 );

    Register = new QPushButton( privateLayoutWidget, "Register" );
    layout19->addWidget( Register );
    layout23->addLayout( layout19 );
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

    useStunYes = new QRadioButton( stunButtonGroup, "useStunYes" );
    stunButtonGroupLayout->addWidget( useStunYes );

    useStunNo = new QRadioButton( stunButtonGroup, "useStunNo" );
    useStunNo->setChecked( TRUE );
    stunButtonGroupLayout->addWidget( useStunNo );
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

    layout7_2 = new QHBoxLayout( 0, 0, 6, "layout7_2"); 

    labelPulseLength = new QLabel( SettingsDTMF, "labelPulseLength" );
    layout7_2->addWidget( labelPulseLength );
    spacer3 = new QSpacerItem( 115, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout7_2->addItem( spacer3 );

    pulseLength = new QSpinBox( SettingsDTMF, "pulseLength" );
    pulseLength->setMaxValue( 1500 );
    pulseLength->setMinValue( 10 );
    pulseLength->setValue( 250 );
    layout7_2->addWidget( pulseLength );
    layout11->addLayout( layout7_2 );

    layout8_2 = new QHBoxLayout( 0, 0, 6, "layout8_2"); 

    labelSendDTMF = new QLabel( SettingsDTMF, "labelSendDTMF" );
    layout8_2->addWidget( labelSendDTMF );
    spacer4 = new QSpacerItem( 85, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout8_2->addItem( spacer4 );

    sendDTMFas = new QComboBox( FALSE, SettingsDTMF, "sendDTMFas" );
    layout8_2->addWidget( sendDTMFas );
    layout11->addLayout( layout8_2 );

    SettingsDTMFLayout->addLayout( layout11, 0, 0 );
    Tab_Signalisations->insertTab( DTMFPage, QString("") );
    layout17->addWidget( Tab_Signalisations );

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
    CodecsChoice->setGeometry( QRect( 20, 10, 126, 196 ) );
    CodecsChoice->setColumnLayout(0, Qt::Vertical );
    CodecsChoice->layout()->setSpacing( 6 );
    CodecsChoice->layout()->setMargin( 11 );
    CodecsChoiceLayout = new QGridLayout( CodecsChoice->layout() );
    CodecsChoiceLayout->setAlignment( Qt::AlignTop );

    layout11_2 = new QHBoxLayout( 0, 0, 6, "layout11_2"); 

    layout9 = new QVBoxLayout( 0, 0, 6, "layout9"); 

    codec1 = new QComboBox( FALSE, CodecsChoice, "codec1" );
    layout9->addWidget( codec1 );

    codec2 = new QComboBox( FALSE, CodecsChoice, "codec2" );
    layout9->addWidget( codec2 );

    codec3 = new QComboBox( FALSE, CodecsChoice, "codec3" );
    layout9->addWidget( codec3 );

    codec4 = new QComboBox( FALSE, CodecsChoice, "codec4" );
    layout9->addWidget( codec4 );

    codec5 = new QComboBox( FALSE, CodecsChoice, "codec5" );
    layout9->addWidget( codec5 );
    layout11_2->addLayout( layout9 );

    layout10_2 = new QVBoxLayout( 0, 0, 6, "layout10_2"); 

    textLabel1_4 = new QLabel( CodecsChoice, "textLabel1_4" );
    layout10_2->addWidget( textLabel1_4 );

    textLabel1_4_2 = new QLabel( CodecsChoice, "textLabel1_4_2" );
    layout10_2->addWidget( textLabel1_4_2 );

    textLabel1_4_3 = new QLabel( CodecsChoice, "textLabel1_4_3" );
    layout10_2->addWidget( textLabel1_4_3 );

    textLabel1_4_4 = new QLabel( CodecsChoice, "textLabel1_4_4" );
    layout10_2->addWidget( textLabel1_4_4 );

    textLabel1_4_5 = new QLabel( CodecsChoice, "textLabel1_4_5" );
    layout10_2->addWidget( textLabel1_4_5 );
    layout11_2->addLayout( layout10_2 );

    CodecsChoiceLayout->addLayout( layout11_2, 0, 0 );
    Tab_Audio->insertTab( CodecsPage, QString("") );

    RingPage = new QWidget( Tab_Audio, "RingPage" );

    ringsChoice = new QComboBox( FALSE, RingPage, "ringsChoice" );
    ringsChoice->setGeometry( QRect( 20, 21, 150, 30 ) );
    Tab_Audio->insertTab( RingPage, QString("") );
    layout17->addWidget( Tab_Audio );

    Tab_Video = new QTabWidget( this, "Tab_Video" );

    DriversPage_2 = new QWidget( Tab_Video, "DriversPage_2" );
    Tab_Video->insertTab( DriversPage_2, QString("") );

    CodecsPage_2 = new QWidget( Tab_Video, "CodecsPage_2" );
    Tab_Video->insertTab( CodecsPage_2, QString("") );
    layout17->addWidget( Tab_Video );

    Tab_Network = new QTabWidget( this, "Tab_Network" );

    DriversPage_3 = new QWidget( Tab_Network, "DriversPage_3" );
    Tab_Network->insertTab( DriversPage_3, QString("") );

    CodecsPage_3 = new QWidget( Tab_Network, "CodecsPage_3" );
    Tab_Network->insertTab( CodecsPage_3, QString("") );
    layout17->addWidget( Tab_Network );

    Tab_Preferences = new QTabWidget( this, "Tab_Preferences" );

    DriversPage_4 = new QWidget( Tab_Preferences, "DriversPage_4" );

    SkinChoice = new QComboBox( FALSE, DriversPage_4, "SkinChoice" );
    SkinChoice->setGeometry( QRect( 12, 42, 110, 27 ) );

    buttonApplySkin = new QPushButton( DriversPage_4, "buttonApplySkin" );
    buttonApplySkin->setGeometry( QRect( 136, 40, 80, 32 ) );
    Tab_Preferences->insertTab( DriversPage_4, QString("") );

    TabPage = new QWidget( Tab_Preferences, "TabPage" );

    QWidget* privateLayoutWidget_2 = new QWidget( TabPage, "layout17" );
    privateLayoutWidget_2->setGeometry( QRect( 10, 10, 262, 200 ) );
    layout17_2 = new QVBoxLayout( privateLayoutWidget_2, 11, 6, "layout17_2"); 

    layout16 = new QHBoxLayout( 0, 0, 6, "layout16"); 

    textLabel1_2 = new QLabel( privateLayoutWidget_2, "textLabel1_2" );
    layout16->addWidget( textLabel1_2 );

    zoneToneChoice = new QComboBox( FALSE, privateLayoutWidget_2, "zoneToneChoice" );
    layout16->addWidget( zoneToneChoice );
    spacer5 = new QSpacerItem( 31, 21, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout16->addItem( spacer5 );
    layout17_2->addLayout( layout16 );

    confirmationToQuit = new QCheckBox( privateLayoutWidget_2, "confirmationToQuit" );
    confirmationToQuit->setChecked( TRUE );
    layout17_2->addWidget( confirmationToQuit );

    checkedTray = new QCheckBox( privateLayoutWidget_2, "checkedTray" );
    layout17_2->addWidget( checkedTray );

    layout16_2 = new QHBoxLayout( 0, 0, 6, "layout16_2"); 

    textLabel1_6 = new QLabel( privateLayoutWidget_2, "textLabel1_6" );
    layout16_2->addWidget( textLabel1_6 );

    voicemailNumber = new QLineEdit( privateLayoutWidget_2, "voicemailNumber" );
    layout16_2->addWidget( voicemailNumber );
    spacer6_2 = new QSpacerItem( 61, 21, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout16_2->addItem( spacer6_2 );
    layout17_2->addLayout( layout16_2 );
    Tab_Preferences->insertTab( TabPage, QString("") );
    layout17->addWidget( Tab_Preferences );

    Tab_About = new QTabWidget( this, "Tab_About" );

    DriversPage_5 = new QWidget( Tab_About, "DriversPage_5" );

    textLabel2_2 = new QLabel( DriversPage_5, "textLabel2_2" );
    textLabel2_2->setGeometry( QRect( 20, 170, 371, 121 ) );

    pixmapLabel1 = new QLabel( DriversPage_5, "pixmapLabel1" );
    pixmapLabel1->setGeometry( QRect( 50, 40, 312, 91 ) );
    pixmapLabel1->setPixmap( image0 );
    pixmapLabel1->setScaledContents( TRUE );
    Tab_About->insertTab( DriversPage_5, QString("") );

    CodecsPage_4 = new QWidget( Tab_About, "CodecsPage_4" );

    pixmapLabel2 = new QLabel( CodecsPage_4, "pixmapLabel2" );
    pixmapLabel2->setGeometry( QRect( 130, 50, 173, 48 ) );
    pixmapLabel2->setPixmap( image1 );
    pixmapLabel2->setScaledContents( TRUE );

    textLabel1 = new QLabel( CodecsPage_4, "textLabel1" );
    textLabel1->setGeometry( QRect( 85, 126, 291, 131 ) );
    Tab_About->insertTab( CodecsPage_4, QString("") );
    layout17->addWidget( Tab_About );

    ConfigurationPanelLayout->addLayout( layout17, 0, 1 );
    languageChange();
    resize( QSize(560, 536).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( Menu, SIGNAL( clicked(QListBoxItem*) ), Tab_Signalisations, SLOT( setFocus() ) );
    connect( buttonCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );
    connect( Menu, SIGNAL( clicked(QListBoxItem*) ), this, SLOT( changeTabSlot() ) );
    connect( buttonSave, SIGNAL( clicked() ), this, SLOT( saveSlot() ) );
    connect( stunButtonGroup, SIGNAL( clicked(int) ), this, SLOT( useStunSlot(int) ) );
    connect( buttonApplySkin, SIGNAL( clicked() ), this, SLOT( applySkinSlot() ) );

    // tab order
    setTabOrder( fullName, userPart );
    setTabOrder( userPart, username );
    setTabOrder( username, password );
    setTabOrder( password, hostPart );
    setTabOrder( hostPart, sipproxy );
    setTabOrder( sipproxy, buttonSave );
    setTabOrder( buttonSave, buttonCancel );
    setTabOrder( buttonCancel, buttonHelp );
    setTabOrder( buttonHelp, SkinChoice );
    setTabOrder( SkinChoice, zoneToneChoice );
    setTabOrder( zoneToneChoice, confirmationToQuit );
    setTabOrder( confirmationToQuit, checkedTray );
    setTabOrder( checkedTray, voicemailNumber );
    setTabOrder( voicemailNumber, useStunNo );
    setTabOrder( useStunNo, STUNserver );
    setTabOrder( STUNserver, playTones );
    setTabOrder( playTones, pulseLength );
    setTabOrder( pulseLength, sendDTMFas );
    setTabOrder( sendDTMFas, Tab_Signalisations );
    setTabOrder( Tab_Signalisations, Menu );
    setTabOrder( Menu, Tab_Audio );
    setTabOrder( Tab_Audio, ossButton );
    setTabOrder( ossButton, codec1 );
    setTabOrder( codec1, codec2 );
    setTabOrder( codec2, codec3 );
    setTabOrder( codec3, codec4 );
    setTabOrder( codec4, codec5 );
    setTabOrder( codec5, Tab_Video );
    setTabOrder( Tab_Video, Tab_Network );
    setTabOrder( Tab_Network, Tab_Preferences );
    setTabOrder( Tab_Preferences, Tab_About );
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
    useStunYes->setText( tr( "Yes" ) );
    useStunNo->setText( tr( "No" ) );
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
    codec4->clear();
    codec4->insertItem( tr( "G711u" ) );
    codec4->insertItem( tr( "G711a" ) );
    codec4->insertItem( tr( "GSM" ) );
    codec5->clear();
    codec5->insertItem( tr( "G711u" ) );
    codec5->insertItem( tr( "G711a" ) );
    codec5->insertItem( tr( "GSM" ) );
    textLabel1_4->setText( tr( "1" ) );
    textLabel1_4_2->setText( tr( "2" ) );
    textLabel1_4_3->setText( tr( "3" ) );
    textLabel1_4_4->setText( tr( "4" ) );
    textLabel1_4_5->setText( tr( "5" ) );
    Tab_Audio->changeTab( CodecsPage, tr( "Codecs" ) );
    Tab_Audio->changeTab( RingPage, tr( "Ringtones" ) );
    Tab_Video->changeTab( DriversPage_2, tr( "Drivers" ) );
    Tab_Video->changeTab( CodecsPage_2, tr( "Codecs" ) );
    Tab_Network->changeTab( DriversPage_3, QString::null );
    Tab_Network->changeTab( CodecsPage_3, QString::null );
    buttonApplySkin->setText( tr( "&Apply" ) );
    buttonApplySkin->setAccel( QKeySequence( tr( "Alt+A" ) ) );
    Tab_Preferences->changeTab( DriversPage_4, tr( "Themes" ) );
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
"Copyright (C) 2004 Savoir-faire Linux inc.<br><br>\n"
"Laurielle LEA &lt;laurielle.lea@savoirfairelinux.com&gt;<br><br>\n"
"SFLPhone-0.3 is released under the General Public License.<br>\n"
"For more information, see http://www.sflphone.org<br>\n"
"</p>" ) );
    Tab_About->changeTab( DriversPage_5, tr( "About SFLPhone" ) );
    textLabel1->setText( tr( "<p align=\"center\">Website: http://www.savoirfairelinux.com<br><br>\n"
"5505, Saint-Laurent - bureau 3030<br>\n"
"Montreal, Quebec H2T 1S6</p>" ) );
    Tab_About->changeTab( CodecsPage_4, tr( "About Savoir-faire Linux inc." ) );
}

