//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AuroPannerMixerFactory.h

/// \file
/// Registers the Auro Panner plugin automatically.
/// This file should be included once in a .CPP (not a .h, really).  The simple inclusion of this file and the linking of the library is enough to use the plugin.
/// <b>WARNING</b>: Include this file only if you wish to link statically with the plugins.  Dynamic Libaries (DLL, so, etc) are automatically detected and do not need this include file.
/// <br><b>Wwise effect name:</b>  Auro Panner
/// <br><b>Library file:</b> AuroPanner.lib

#ifndef _AUROPANNERMIXERFACTORY_H_
#define _AUROPANNERMIXERFACTORY_H_

#if (defined AK_WIN && !defined AK_WINPHONE && (!defined( AK_USE_METRO_API ) || _MSC_VER >= 1900)) || defined AK_PS4 || defined AK_XBOXONE  || defined AK_ANDROID

AK_STATIC_LINK_PLUGIN(AuroPannerMixer)
AK_STATIC_LINK_PLUGIN(AuroPannerMixerAttachment)
#endif
#endif // _AUROPANNERMIXERFACTORY_H_

