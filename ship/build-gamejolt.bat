cd "%~dp0"
rm lasercrabs-win.zip lasercrabs-mac.zip lasercrabs-linux.zip
rm -rf final final-mac final-linux
cp -r ../final ./final
cp -r ../final-mac ./final-mac
cp -r ../final-linux ./final-linux
cp gamejolt-en/build.txt final/
cp gamejolt-en/build.txt "final-mac/LASERCRABS.app/Contents/MacOS/"
cp gamejolt-en/build.txt final-linux/
powershell.exe -command "Compress-Archive -Path final/* -DestinationPath lasercrabs-win.zip"
powershell.exe -command "Compress-Archive -Path final-mac/* -DestinationPath lasercrabs-mac.zip"
powershell.exe -command "Compress-Archive -Path final-linux/* -DestinationPath lasercrabs-linux.zip"
rm -rf final final-mac final-linux
