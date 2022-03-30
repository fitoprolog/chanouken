@echo off

set PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin
call windows-prebuild-vs2017.bat
MSBUILD.EXE /t:Build /m /p:PreferredToolArchitecture=x64 /p:Configuration=Release;Platform=x64 %CD%\build-vs2017\CoolVLViewer.sln

echo Compilation finished (and hopefully successful). Press a key to exit.
pause
