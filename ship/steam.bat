cd "%~dp0"
..\..\ContentBuilder\builder\steamcmd.exe +login "%1" "%2" +run_app_build %~dp0/app_build_728100.vdf +quit