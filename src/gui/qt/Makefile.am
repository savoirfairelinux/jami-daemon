libexec_PROGRAMS = sflphone-qt

BUILT_SOURCES = \
    ./ConfigurationPanelui.cpp \
    ./ConfigurationPanel.h \
    ./ConfigurationPanelmocpp.cpp \
    ./ConfigurationManagerImplmoc.cpp \
    ./ConfigurationPanelImplmoc.cpp \
    ./JPushButtonmoc.cpp \
    ./Launchermoc.cpp \
    ./NumericKeypadmoc.cpp \
    ./PhoneLinemoc.cpp \
    ./PhoneLineButtonmoc.cpp \
    ./PhoneLineManagerImplmoc.cpp \
    ./SFLLcdmoc.cpp \
    ./SFLPhoneAppmoc.cpp \
    ./SFLPhoneWindowmoc.cpp \
    ./SFLRequestmoc.cpp \
    ./Requestmoc.cpp \
    ./SessionIOmoc.cpp \
    ./TCPSessionIOmoc.cpp \
    ./TransparentWidgetmoc.cpp \
    ./VolumeControlmoc.cpp \
    ./qmake_image_collection.cpp

IMAGES = $(top_srcdir)/images/*.png

sflphone_qt_SOURCES = \
    $(BUILT_SOURCES) \
    ./Account.cpp \
    ./Account.hpp \
    ./Call.cpp \
    ./Call.hpp \
    ./CallManager.hpp \
    ./CallManagerImpl.cpp \
    ./CallManagerImpl.hpp \
    ./CallStatus.cpp \
    ./CallStatus.hpp \
    ./CallStatusFactory.hpp \
    ./ConfigurationManager.hpp \
    ./ConfigurationManagerImpl.cpp \
    ./ConfigurationManagerImpl.hpp \
    ./ConfigurationPanelImpl.cpp \
    ./ConfigurationPanelImpl.hpp \
    ./DebugOutput.hpp \
    ./DebugOutputImpl.cpp \
    ./DebugOutputImpl.hpp \
    ./Event.cpp \
    ./Event.hpp \
    ./EventFactory.hpp \
    ./EventFactory.inl \
    ./Factory.hpp \
    ./Factory.inl \
    ./JPushButton.cpp JPushButton.hpp \
    ./Launcher.cpp \
    ./Launcher.hpp \
    ./NumericKeypad.cpp \
    ./NumericKeypad.hpp \
    ./ObjectFactory.hpp \
    ./ObjectFactory.inl \
    ./ObjectPool.hpp \
    ./ObjectPool.inl \
    ./PhoneLine.cpp \
    ./PhoneLine.hpp \
    ./PhoneLineButton.cpp \
    ./PhoneLineButton.hpp \
    ./PhoneLineLocker.cpp \
    ./PhoneLineLocker.hpp \
    ./PhoneLineManager.hpp \
    ./PhoneLineManagerImpl.cpp \
    ./PhoneLineManagerImpl.hpp \
    ./QjListBoxPixmap.cpp \
    ./QjListBoxPixmap.hpp \
    ./Request.cpp \
    ./Request.hpp \
    ./Requester.hpp \
    ./RequesterImpl.cpp \
    ./RequesterImpl.hpp \
    ./RequesterImpl.inl \
    ./SFLCallStatus.hpp \
    ./SFLEvents.cpp \
    ./SFLEvents.hpp \
    ./SFLLcd.cpp \
    ./SFLLcd.hpp \
    ./SFLPhoneApp.cpp \
    ./SFLPhoneApp.hpp \
    ./SFLPhoneWindow.cpp \
    ./SFLPhoneWindow.hpp \
    ./SFLRequest.cpp \
    ./SFLRequest.hpp \
    ./Session.cpp \
    ./Session.hpp \
    ./SessionFactory.hpp \
    ./SessionIO.hpp \
    ./SessionIOFactory.hpp \
    ./SkinManager.hpp \
    ./SkinManagerImpl.cpp \
    ./SkinManagerImpl.hpp \
    ./TCPSessionIO.cpp \
    ./TCPSessionIO.hpp \
    ./TCPSessionIOCreator.cpp \
    ./TCPSessionIOCreator.hpp \
    ./TransparentWidget.cpp \
    ./TransparentWidget.hpp \
    ./Url.cpp \
    ./Url.hpp \
    ./VolumeControl.cpp \
    ./VolumeControl.hpp \
    ./globals.h \
    ./main.cpp

CLEANFILES = \
	$(BUILT_SOURCES)

sflphone_qt_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DDATADIR=\"$(sflphone_datadir)\"
sflphone_qt_LDADD = $(top_srcdir)/libs/taxidermy/libtaxidermy.la

AM_CPPFLAGS = -I$(top_srcdir)/libs/ $(QT_CXXFLAGS) $(X_INCLUDES) $(all_includes)
AM_LDFLAGS = $(QT_LDADD) $(X_LDFLAGS) $(all_libraries)


qmake_image_collection.cpp: $(IMAGES)
	$(UIC) -embed sflphone-qt $(IMAGES) -o $@

%.h: %.ui
	$(UIC) -o $@ $<

%mocpp.cpp: %.h
	$(MOC) -o $@ $<

%moc.cpp: %.hpp
	$(MOC) -o $@ $<

%ui.cpp: %.ui
	$(UIC) -o $@ -impl $*.h $<


