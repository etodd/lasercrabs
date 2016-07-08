@echo off
rm -rf final
mkdir final
cp build/Release/yearning.exe final
cp -R build/assets/ final/assets
cp build/gamecontrollerdb.txt final
cp language.txt final
cp shipme.txt final/readme.txt
butler push ./final et1337/yearning:win
pause