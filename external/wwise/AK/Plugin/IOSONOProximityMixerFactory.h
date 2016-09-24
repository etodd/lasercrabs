//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// IOSONOProximityMixerFactory.h

/// \file
/// Registers the IOSONO plugin automatically.
/// This file should be included once in a .CPP (not a .h, really).  The simple inclusion of this file and the linking of the library is enough to use the plugin.
/// <b>WARNING</b>: Include this file only if you wish to link statically with the plugins.  Dynamic Libaries (DLL, so, etc) are automatically detected and do not need this include file.
/// <br><b>Wwise effect name:</b>  IOSONO Proximity
/// <br><b>Library file:</b> IOSONOProximityMixer.lib
#if (defined AK_WIN && !defined AK_WINPHONE && !defined AK_USE_METRO_API) || defined AK_PS4 || defined AK_XBOXONE
AK_STATIC_LINK_PLUGIN(IOSONOProximityMixer)
AK_STATIC_LINK_PLUGIN(IOSONOProximityFXAttachment)
#endif

