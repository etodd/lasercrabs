@echo off
rm -rf final
mkdir final
mkdir final/mod
cp build/Release/yearning.exe final
cp build/Release/import.exe final
cp -R build/assets/ final/assets
cp -R assets/script/ final/script
cp build/gamecontrollerdb.txt final
cp language.txt final
cp shipme.txt final/readme.txt
butler push ./final et1337/yearning:win
pause