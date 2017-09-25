rm -rf final
mkdir final
set /p config="Enter config name: "
cp build/%config%/deceiver.exe final
cp -R build/assets/ final/assets
cp build/gamecontrollerdb.txt final
cp .itch-win.toml final/.itch.toml
cp build.txt final
cp shipme.txt final/readme.txt
pause
