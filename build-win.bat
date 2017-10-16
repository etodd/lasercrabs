rm -rf final
mkdir final
set /p config="Enter config name: "
cp build/%config%/deceiver.exe final
cp -R build/assets/ final/assets
cp build/gamecontrollerdb.txt final
cp build/steam_api64.dll final
cp .itch-win.toml final/.itch.toml
cp build.txt final
cp shipme.txt final/readme.txt


FOR /F "tokens=* USEBACKQ" %%F IN (`git describe --always --tags`) DO (
set version=%%F
)
mkdir pdb
cp build/%config%/deceiver.pdb pdb/%version%.pdb

pause
