@echo off
call where /q blender
if not %errorlevel% == 0 (
	echo Please install Blender 2.75a or higher and make sure it's on the system path!
	pause
	goto :eof
)
call where /q WwiseCLI
if not %errorlevel% == 0 (
	echo Please install Wwise and make sure it's on the system path!
	pause
	goto :eof
)
git submodule update --init --recursive
mkdir build
pushd build
cmake .. -G "Visual Studio 15 2017 Win64" -DSERVER=1 -DCLIENT=1
popd
pause