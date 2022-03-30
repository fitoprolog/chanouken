@echo off

python scripts\develop.py -G vs2019-vc141 -t Release

echo.
echo Please, be aware that vs2019 viewer builds are currently unsupported.
echo.

echo Configuring done. Press a key to exit.
pause
