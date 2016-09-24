//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2010 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkAACFactory.h

/// \file
/// Codec plug-in unique ID and creation functions (hooks) necessary to register the AAC codec in the sound engine.

#ifndef _AK_AACFACTORY_H_
#define _AK_AACFACTORY_H_

#ifdef AK_APPLE

AK_STATIC_LINK_PLUGIN(AkAACDecoder)
#endif

#endif // _AK_AACFACTORY_H_
