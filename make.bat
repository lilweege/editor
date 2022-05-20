@echo off
setlocal

:: either run this script from an msvc-enabled developer console, or find and invoke your vcvars batch script, for instance:
:: `call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64`
where /q cl || (
    echo ERROR: "cl" not found - please run this from the MSVC x64 native tools command prompt.
    exit /b 1
)

:: NOTE: don't overwrite %INCLUDE%
set INCLUDES=/Ilib\SDL2-2.0.22\include /D_REENTRANT /Ilib\glew-2.1.0\include
set LIBS=lib\SDL2-2.0.22\lib\x64\SDL2.lib lib\SDL2-2.0.22\lib\x64\SDL2main.lib ^
    lib\glew-2.1.0\lib\Release\x64\glew32.lib ^
    shell32.lib opengl32.lib
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
    if exist bin\glew32.dll del bin\glew32.dll

    :: download and extract libs
    curl --output-dir lib -o SDL2.zip -fsSLO https://www.libsdl.org/release/SDL2-devel-2.0.22-VC.zip
    tar -x -f lib\SDL2.zip -C lib
    del lib\SDL2.zip
    mkdir lib\SDL2-2.0.22\include\SDL2
    move lib\SDL2-2.0.22\include\*.* lib\SDL2-2.0.22\include\SDL2 >nul
    move lib\SDL2-2.0.22\lib\x64\SDL2.dll bin >nul

    curl --output-dir lib -o glew.zip -fsSLO https://sourceforge.net/projects/glew/files/glew/2.1.0/glew-2.1.0-win32.zip/download
    tar -x -f lib\glew.zip -C lib
    del lib\glew.zip
    move lib\glew-2.1.0\bin\Release\x64\glew32.dll bin >nul

goto :EOF

:clean
    del bin\editor.exe obj\*.obj obj\*.pdb bin\*.pdb bin\*.ilk *.obj *.exe >nul 2>&1
goto :EOF
