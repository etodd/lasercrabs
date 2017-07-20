rm -rf final
mkdir final
cd final
mkdir mod
cd ..
cp build/RelWithDebInfo/deceiver.exe final
cp build/RelWithDebInfo/import.exe final
cp -R build/assets/ final/assets
cp -R assets/script/ final/script
rm -rf final/script/etodd_blender_fbx/__pycache__
cp build/gamecontrollerdb.txt final
cp .itch-win.toml final/.itch.toml
cp language.txt final
cp shipme.txt final/readme.txt
pause