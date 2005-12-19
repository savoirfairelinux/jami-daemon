noinst_LTLIBRARIES = libtaxidermy.la

BUILT_SOURCES = \
        EventFiltermoc.cpp \
           Huntermoc.cpp \
           PaintEventFiltermoc.cpp \
           QButtonBuildermoc.cpp \
           WidgetBuilderCreatormoc.cpp WidgetBuilderCreator.inl \
           WidgetBuilderFactorymoc.cpp \
           WidgetBuilderFactoryImplmoc.cpp WidgetBuilderFactoryImpl.inl \
           WidgetBuildermoc.cpp 


noinst_HEADERS = \
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


libtaxidermy_la_SOURCES = \
	$(BUILT_SOURCES) \
	EventFilter.cpp \
           Hunter.cpp \
           PaintEventFilter.cpp \
           QButtonBuilder.cpp \
           Taxidermist.cpp \
           WidgetBuilder.cpp \
           WidgetBuilderFactoryImpl.cpp \
           config.cpp \
           qtutils.cpp

CLEANFILES = \
	$(BUILT_SOURCES)


KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = -I$(top_srcdir)/libs/ $(QT_INCLUDES) $(X_INCLUDES) 
AM_LDFLAGS = $(QT_LDFLAGS) $(X_LDFLAGS) 



%.h: %.ui
	$(UIC) -o $@ $<

moc_%.cpp: %.h
	$(MOC) -o $@ $<

%moc.cpp: %.hpp
	$(MOC) -o $@ $<

%ui.cpp: %.ui
	$(UIC) -o $@ -impl $*.h $<



#UI_DIR = bin/ui
#MOC_DIR = bin/moc
#OBJECTS_DIR = bin/obj


