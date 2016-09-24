#pragma once
/// \file
/// Registers the Wwise Motion Generator plugin automatically.
/// This file should be included once in a .CPP (not a .h, really).  The simple inclusion of this file and the linking of the library is enough to use the plugin.
/// <b>WARNING</b>: Include this file only if you wish to link statically with the plugins.  Dynamic Libaries (DLL, so, etc) are automatically detected and do not need this include file.
/// <br><b>Wwise plugin name:</b>  Wwise Motion Generator
/// <br><b>Library file:</b> AkMotionGenerator.lib

#include <AK/SoundEngine/Common/AkTypes.h>
#if defined AK_MOTION
AK_STATIC_LINK_PLUGIN(AkMotionGenerator)
#endif
