//////////////////////////////////////////////////////////////////////
//
// AkStreamingDefaults.h
//
// Default values for streaming and I/O device settings.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_STREAMING_DEFAULTS_H_
#define _AK_STREAMING_DEFAULTS_H_

#include "AkPlatformStreamingDefaults.h"

// Stream Manager Settings.
#define AK_DEFAULT_STM_OBJ_POOL_SIZE		(64*1024)		// 64 KB for small objects pool, shared across all devices.
															// Small objects are the manager, devices, stream objects, pending transfers, buffer records, and so on.
															// Ideally, this pool should never run out of memory, because it may cause undesired I/O transfers 
															// cancellation, or CPU spikes. I/O memory should be bound by the size of each device's I/O pool instead.

// Device settings.
#define AK_DEFAULT_DEVICE_IO_POOL_SIZE		(2*1024*1024)	// 2 MB for I/O. Pool is split up in blocks of size AkDeviceSettings::uGranularity.
															// The smaller the granularity, the smaller this pool can be to handle the same number of streams.
															// However, having a small granularity is often inefficient regarding I/O throughput.
															// As a rule of thumb, use the smallest granularity that does not degrade I/O throughput.
															// Then adjust the I/O pool size in order to handle the number number of streams you expect to be using.
															// Consider that each stream will be at least double or triple-buffered (in fact, this depends on the target buffering length).
#define AK_DEFAULT_DEVICE_GRANULARITY		(16*1024)		// 16 KB. Completely arbitrary (see note above).
#define AK_DEFAULT_DEVICE_SCHEDULER			(AK_SCHEDULER_BLOCKING) // Blocking device: the simplest regarding the Low-Level IO.

#define AK_DEFAULT_DEVICE_THREAD_PRIORITY	(AK_THREAD_PRIORITY_ABOVE_NORMAL)	// I/O thread spends most of its time sleeping or waiting for a device. 
															// It should have a high priority, in order to quickly determine the next task and send it to the 
															// Low-Level IO, then wait for it.

#define AK_DEFAULT_DEVICE_BUFFERING_LENGTH	(380.f)			// 380 ms. 
#define AK_DEFAULT_MAX_CONCURRENT_IO		(8)				// 8. With AK_SCHEDULER_BLOCKING, it is always 1 anyway. Default is arbitrarily set to 8 for deferred device.

#define AK_DEFAULT_DEVICE_CACHE_ENABLED	(false)			// Caching is disabled by default.

#endif //_STREAMING_DEFAULTS_H_
