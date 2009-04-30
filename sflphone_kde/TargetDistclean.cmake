 
# add custom target distclean
# cleans and removes cmake generated files etc.
# Jan Woetzel 04/2003
#

IF (UNIX)
  ADD_CUSTOM_TARGET (distclean @echo cleaning for source distribution)
  SET(DISTCLEANED
   cmake.depends
   cmake.check_depends
   CMakeCache.txt
   cmake.check_cache
   *.cmake
   Makefile
   core core.*
   gmon.out
   *~
   *_automoc.cpp.files
   *_automoc.cpp
   moc_*.cpp
   ui_*.h
   *.moc
   qrc_resources.cxx
   *introspecinterface.cpp
   *introspecinterface.h
   sflphone-client-kde
   sflphone-client-kde.shell
  )
  
  ADD_CUSTOM_COMMAND(
    DEPENDS clean
    COMMENT "distribution clean"
    COMMAND rm
    ARGS    -Rf CMakeTmp CMakeFiles ${DISTCLEANED}
    TARGET  distclean
  )
ENDIF(UNIX)


