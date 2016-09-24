#pragma once
/// \file
/// Registers the SoundSeed Air/SoundSeed Wind plugin automatically.
/// This file should be included once in a .CPP (not a .h, really).  The simple inclusion of this file and the linking of the library is enough to use the plugin.
/// <b>WARNING</b>: Include this file only if you wish to link statically with the plugins.  Dynamic Libaries (DLL, so, etc) are automatically detected and do not need this include file.
/// <br><b>Wwise plugin name:</b>  SoundSeed Air/SoundSeed Wind
/// <br><b>Library file:</b> AkSoundSeedWind.lib


#if !defined AK_3DS
AK_STATIC_LINK_PLUGIN(AkSoundSeedWind)
#endif
