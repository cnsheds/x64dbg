@echo off

if "%OLDPATH%"=="" set OLDPATH=%PATH%

if "%QT32PATH%"=="" set QT32PATH=D:\Qt\Qt5.6.2\5.6\msvc2015\bin
if "%QT64PATH%"=="" set QT64PATH=D:\Qt\Qt5.6.2-x64\5.6\msvc2015_64\bin
if "%QTCREATORPATH%"=="" set QTCREATORPATH=d:\Qt\qtcreator-4.3.1\bin
if "%VSVARSALLPATH%"=="" set VSVARSALLPATH=c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat
if "%COVERITYPATH%"=="" set COVERITYPATH=c:\coverity\bin

if "%1"=="x32" (
    goto x32
) else if "%1"=="x64" (
    goto x64
) else if "%1"=="coverity" (
    goto coverity
) else (
    echo "Usage: setenv x32/x64/coverity"
    goto :eof
)

:x32
echo Setting Qt in PATH
set PATH=%PATH%;%QT32PATH%
set PATH=%PATH%;%QTCREATORPATH%
echo Setting VS in PATH
call "%VSVARSALLPATH%"
goto :eof

:x64
echo Setting Qt in PATH
set PATH=%PATH%;%QT64PATH%
set PATH=%PATH%;%QTCREATORPATH%
echo Setting VS in PATH
call "%VSVARSALLPATH%" amd64
goto :eof

:coverity
echo Setting Coverity in PATH
set PATH=%PATH%;%COVERITYPATH%
goto :eof