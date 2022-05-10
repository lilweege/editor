@echo off

echo %PATH% | find /c /i "msvc" > nul
if "%errorlevel%" == "0" goto hasCL
    echo Script must be run from an msvc-enabled terminal.
    :: this may require some manual intervension from the user
    :: either run this script from an msvc-enabled developer console,
    :: or 'enabling' your terminal by running one of the following commands
    :: (for some reason microsoft enjoys making things difficult so the exact path depends on the version)
    :: `call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64`
    goto :EOF
:hasCL

:: NOTE: don't overwrite %INCLUDE%
set INCLUDES=/Ilib\SDL2-2.0.22\include /D_REENTRANT
set LIBS=/LIBPATH:lib\SDL2-2.0.22\lib\x64 SDL2.lib SDL2main.lib shell32.lib
:: TODO: figure out why SDL2 doesn't work with /std:c11
set CFLAGS_COMMON=/nologo /W4 /wd4996 /wd4200 /TC
set CFLAGS_DEBUG=/Zi /Od
set CFLAGS_RELEASE=/O2 /DNDEBUG
set CFLAGS=%CFLAGS_COMMON% %CFLAGS_DEBUG%
set SUBSYS=CONSOLE


if "%1%" == ""         goto build
if "%1%" == "debug"    goto build
if "%1%" == "release"  goto release
if "%1%" == "install"  goto install
if "%1%" == "clean"    goto clean
echo Invalid argument '%1%'
goto :EOF

:release
    set CFLAGS=%CFLAGS_COMMON% %CFLAGS_RELEASE%
    set SUBSYS=WINDOWS
:build
    if exist lib goto ready
        echo Missing dependencies, run `make install` first.
        goto :EOF
    :ready
    cl %CFLAGS% %INCLUDES% /Febin\editor /Fdbin\vc /Fo.\obj\ src\*.c /link %LIBS% /SUBSYSTEM:%SUBSYS%
goto :EOF

:install
    :: reset lib dir
    if exist lib rmdir /s /q lib
    mkdir lib
    if exist bin\SDL2.dll del bin\SDL2.dll
    :: download and extract libs
    curl -fsSLO https://www.libsdl.org/release/SDL2-devel-2.0.22-VC.zip --output-dir lib
    tar -x -f lib\SDL2-devel-2.0.22-VC.zip -C lib
    del lib\SDL2-devel-2.0.22-VC.zip
    mkdir lib\SDL2-2.0.22\include\SDL2
    move lib\SDL2-2.0.22\include\*.* lib\SDL2-2.0.22\include\SDL2 >nul
    move lib\SDL2-2.0.22\lib\x64\SDL2.dll bin >nul
goto :EOF

:clean
    del bin\editor.exe obj\*.obj obj\*.pdb bin\*.pdb bin\*.ilk *.obj *.exe >nul 2>&1
goto :EOF
