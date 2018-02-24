cd "%~dp0"
rm deceiver-win.zip deceiver-mac.zip deceiver-linux.zip
rm -rf final final-mac final-linux
cp -r ../final ./final
cp -r ../final-mac ./final-mac
cp -r ../final-linux ./final-linux
cp gamejolt-en/build.txt final
cp gamejolt-en/build.txt final-mac/
cp gamejolt-en/build.txt final-linux/
powershell.exe -command "Compress-Archive -Path final/* -DestinationPath deceiver-win.zip"
powershell.exe -command "Compress-Archive -Path final-mac/* -DestinationPath deceiver-mac.zip"
powershell.exe -command "Compress-Archive -Path final-linux/* -DestinationPath deceiver-linux.zip"
rm -rf final final-mac final-linux
