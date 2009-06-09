 
# add custom target distclean
# cleans and removes cmake generated files etc.
# Jan Woetzel 04/2003
#

IF (UNIX)
  ADD_CUSTOM_TARGET (distclean @echo cleaning for source distribution)
  SET(DISTCLEANED
#    cmake.depends
#    cmake.check_depends
#    CMakeCache.txt
#    cmake.check_cache
#    *.cmake
#    Makefile
#    core core.*
#    gmon.out
#    *~
#    *_automoc.cpp.files
#    *_automoc.cpp
#    moc_*.cpp
#    ui_*.h
#    *.moc
#    qrc_resources.cxx
#    *_dbus_interface.cpp
#    *_dbus_interface.h
#    sflphone-client-kde
#    sflphone-client-kde.shell
#    install_manifest.txt
#    *.1
   CMakeCache.txt
   *.cmake
   Makefile
   CMakeFiles
   CMakeTmp
   install_manifest.txt
   man/*.cmake
   man/Makefile
   man/CMakeFiles
   man/*.html
   man/*.1
   doc/*.cmake
   doc/Makefile
   doc/CMakeFiles
   doc/*.bz2
   data/*.cmake
   data/Makefile
   data/CMakeFiles
   src/*.cmake
   src/Makefile
   src/CMakeFiles
   src/*.o
   src/moc_*.cpp
   src/ui_*.h
   src/qrc_*.cxx
   src/*_dbus_interface.cpp
   src/*_dbus_interface.h
   src/*_automoc.cpp.files
   src/*_automoc.cpp
   src/sflphone-client-kde
   src/sflphone-client-kde.shell
   src/*.moc
   po/*.cmake
   po/Makefile
   po/CMakeFiles
  )
  
  SET(DISTCLEANED_REC
   *.cmake 
   Makefile
   CMakeFiles
  )
  
  ADD_CUSTOM_COMMAND(
    DEPENDS clean
    COMMENT "distribution clean"
    COMMAND rm
    ARGS    -Rf CMakeTmp CMakeFiles ${DISTCLEANED} `find ./po  -maxdepth 2  -name 'Makefile'` `find ./po  -maxdepth 2  -name 'CMakeFiles'` `find ./po  -maxdepth 2  -name '*.gmo'` `find ./po  -maxdepth 2  -name '*.cmake'`
#     COMMAND find
#     ARGS "./po ( -name 'Makefile' -o -name 'CMakeFiles' -o -name '*.gmo' -o -name '*.cmake' ) -maxdepth 2 -exec rm -rf {} \;"
#     COMMAND rm 
#     ARGS -Rf `find ./po  -maxdepth 2  -name 'Makefile'`
#     COMMAND find
#     ARGS ./po  -maxdepth 2  -name 'CMakeFiles' -exec rm -rf {} \;
#     COMMAND find
#     ARGS ./po  -maxdepth 2  -name '*.gmo'      -exec rm -rf {} \;
#     COMMAND find
#     ARGS ./po  -maxdepth 2  -name '*.cmake'    -exec rm -rf {} \;
#     COMMAND find
#     ARGS ". \( -name 'Makefile' -o -name 'CMakeFiles' \) -exec rm -rf {} \;"
    TARGET  distclean
  )
ENDIF(UNIX)


