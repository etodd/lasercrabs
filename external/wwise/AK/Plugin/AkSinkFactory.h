//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSinkFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the mixer plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  Sink
/// <br><b>Library file:</b> AkSink.lib

#ifndef _AK_SINK_FACTORY_H_
#define _AK_SINK_FACTORY_H_



/// \file
/// Registers the Audio Sink Sample plugin automatically.
/// This file should be included once in a .CPP (not a .h, really).  The simple inclusion of this file and the linking of the library is enough to use the plugin.
/// <b>WARNING</b>: Include this file only if you wish to link statically with the plugins.  Dynamic Libaries (DLL, so, etc) are automatically detected and do not need this include file.
/// <br><b>Wwise plugin name:</b>  Audio Sink Sample
/// <br><b>Library file:</b> AkSink.lib

const unsigned long AKEFFECTID_SINK = 152;
#if (defined AK_WIN && !defined AK_WINPHONE && !defined AK_USE_METRO_API)
AK_STATIC_LINK_PLUGIN(AkSink)
#endif

#endif // _AK_SINK_FACTORY_H_

