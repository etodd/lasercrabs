//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// SceAudio3dMixerFactory.h

/// \file
/// Plug-in unique ID and creation functions (hooks) necessary to register the mixer plug-in in the sound engine.
/// <br><b>Wwise effect name:</b>  SCE Audio3d
/// <br><b>Library file:</b> SceAudioAudio3dEngine.lib

#ifndef _SCE_AUDIO_3D_MIXER_FACTORY_H_
#define _SCE_AUDIO_3D_MIXER_FACTORY_H_

#if defined AK_PS4
AK_STATIC_LINK_PLUGIN(SceAudio3dMixer)
AK_STATIC_LINK_PLUGIN(SceAudio3dMixerAttachment)
AK_STATIC_LINK_PLUGIN(SceAudio3dSinkEffect)
#endif


#endif // _SCE_AUDIO_3D_MIXER_FACTORY_H_

