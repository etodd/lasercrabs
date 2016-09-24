/// \file 
/// Registers the AudioInput plugin automatically.
/// This file should be included once in a .CPP (not a .h, really).  The simple inclusion of this file and the linking of the library is enough to use the plugin.
/// <b>WARNING</b>: Include this file only if you wish to link statically with the plugins.  Dynamic Libaries (DLL, so, etc) are automatically detected and do not need this include file.
/// <br><b>Wwise source name:</b>  AudioInput
/// <br><b>Library file:</b> AkAudioInputSource.lib

AK_STATIC_LINK_PLUGIN(AkAudioInputSource)

#include <AK/Plugin/AkAudioInputPlugin.h>
