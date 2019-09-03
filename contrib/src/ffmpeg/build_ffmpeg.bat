set MSYS2_PATH_TYPE=inherit
IF exist C:\msys64\usr\bin ( set MSYS2_BIN="C:\msys64\usr\bin\bash.exe" ) ELSE ( set MSYS2_BIN="C:tools\msys64\usr\bin\bash.exe" )
%MSYS2_BIN% --login -x %CONTRIB_SRC_DIR%\ffmpeg\windows-configure-make.sh win32 x64
