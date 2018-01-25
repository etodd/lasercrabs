cd ..
rm -rf final
mkdir final
cp build/RelWithDebInfo/deceiver.exe final
cp -R build/assets/ final/assets
cp build/gamecontrollerdb.txt final
cp build/steam_api64.dll final
cp ship/.itch-win.toml final/.itch.toml
cp ship/build.txt final
cp ship/shipme.txt final/readme.txt


FOR /F "tokens=* USEBACKQ" %%F IN (`git describe --always --tags`) DO (
set version=%%F
)
mkdir pdb
cp build/RelWithDebInfo/deceiver.pdb pdb/%version%.pdb
cp build/RelWithDebInfo/deceiver.exe pdb/%version%.exe

pause
