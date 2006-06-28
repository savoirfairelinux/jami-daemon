noinst_LTLIBRARIES = libtaxidermy.la

BUILT_SOURCES = \
        EventFiltermoc.cpp \
           Huntermoc.cpp \
           PaintEventFiltermoc.cpp \
           QButtonBuildermoc.cpp \
           QWidgetBuildermoc.cpp \
           WidgetBuilderCreatormoc.cpp \
           WidgetBuilderFactorymoc.cpp \
           WidgetBuilderFactoryImplmoc.cpp \
           WidgetBuildermoc.cpp 


noinst_HEADERS = \
           EventFilter.hpp \
           Hunter.hpp \
           PaintEventFilter.hpp \
           QWidgetBuilder.hpp \
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
           QWidgetBuilder.cpp \
           QButtonBuilder.cpp \
           Taxidermist.cpp \
           WidgetBuilder.cpp \
           WidgetBuilderFactoryImpl.cpp \
           config.cpp \
           qtutils.cpp

CLEANFILES = \
	$(BUILT_SOURCES)


AM_CPPFLAGS = -I$(top_srcdir)/libs/ $(QT_CXXFLAGS) $(X_INCLUDES) 
AM_LDFLAGS = $(QT_LDADD) $(X_LDFLAGS) 
libtaxidermy_la_LIBADD = $(QT_LDADD)



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


