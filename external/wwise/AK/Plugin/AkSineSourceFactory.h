#pragma once
/// \file
/// Plug-in function necessary to link the Wwise Sine plug-in in the sound engine.
/// <strong>WARNING</strong>: Include this file only if you wish to link statically with the plugins.  Dynamic Libaries (DLL, so, etc) are automatically detected and do not need this include file.
/// <br><b>Wwise plugin name:</b>  Wwise Sine
/// <br><b>Library file:</b> AkSineSource.lib


AK_STATIC_LINK_PLUGIN(SineSource)
