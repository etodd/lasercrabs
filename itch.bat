@echo off
rm -rf final
mkdir final
cd final
mkdir mod
cd ..
cp build/Release/deceiver.exe final
cp build/Release/import.exe final
cp -R build/assets/ final/assets
cp -R assets/script/ final/script
rm -rf final/script/etodd_blender_fbx/__pycache__
cp build/gamecontrollerdb.txt final
cp .itch.toml final
cp language.txt final
cp shipme.txt final/readme.txt
butler push ./final etodd/deceiver:win
pause