//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkVorbisFactory.h

/// \file
/// Codec plug-in unique ID and creation functions (hooks) necessary to register the Vorbis codec in the sound engine.

#ifndef _AK_VORBISFACTORY_H_
#define _AK_VORBISFACTORY_H_

class IAkSoftwareCodec;

/// Prototype of the Vorbis codec bank source creation function.
AK_FUNC( IAkSoftwareCodec*, CreateVorbisBankPlugin )( 
	void* in_pCtx			///< Bank source decoder context
	);

/// Prototype of the Vorbis codec file source creation function.
AK_FUNC( IAkSoftwareCodec*, CreateVorbisFilePlugin )( 
	void* in_pCtx 			///< File source decoder context
	);

/*
Use the following code to register this codec:

	AK::SoundEngine::RegisterCodec(
		AKCOMPANYID_AUDIOKINETIC,
		AKCODECID_VORBIS,
		CreateVorbisFilePlugin,
		CreateVorbisBankPlugin );
*/

#endif // _AK_VORBISFACTORY_H_
