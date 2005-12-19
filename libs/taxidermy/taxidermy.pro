TEMPLATE = lib
LANGUAGE = C++

CONFIG += qt stl warn_on debug staticlib

TARGET = taxidermy
#DESTDIR = bin
#DLLDESTDIR = tests/demo1/bin/styles

#INCLUDEPATH += ../util-ng/include
#INCLUDEPATH += ../qtutil/include
INCLUDEPATH += ../

HEADERS += ButtonBuilder.hpp \
           EventFilter.hpp \
           Hunter.hpp \
           PaintEventFilter.hpp \
           QButtonBuilder.hpp \
           Taxidermist.hpp \
           WidgetBuilderCreator.hpp WidgetBuilderCreator.inl \
           WidgetBuilderFactory.hpp \
           WidgetBuilderFactoryImpl.hpp WidgetBuilderFactoryImpl.inl \
           WidgetBuilder.hpp \
           config.h \
           qtutils.hpp


SOURCES += EventFilter.cpp \
           Hunter.cpp \
           PaintEventFilter.cpp \
           QButtonBuilder.cpp \
           Taxidermist.cpp \
           WidgetBuilder.cpp \
           WidgetBuilderFactoryImpl.cpp \
           config.cpp \
           qtutils.cpp

#UI_DIR = bin/ui
#MOC_DIR = bin/moc
#OBJECTS_DIR = bin/obj


