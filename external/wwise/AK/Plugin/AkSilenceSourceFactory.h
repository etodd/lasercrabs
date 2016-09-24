#pragma once
/// \file
/// Plug-in function necessary to link the Wwise Silence plug-in in the sound engine.
/// <b>WARNING</b>: Include this file only if you wish to link statically with the plugins.  Dynamic Libaries (DLL, so, etc) are automatically detected and do not need this include file.
/// <br><b>Wwise plugin name:</b>  Wwise Silence
/// <br><b>Library file:</b> AkSilenceSource.lib


AK_STATIC_LINK_PLUGIN(AkSilenceSource)
AK_STATIC_LINK_PLUGIN(AkSilenceSourceMotion)
