//////////////////////////////////////////////////////////////////////
//
// AkDeviceBase.cpp
//
// Device implementation that is common across all IO devices.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include "AkDeviceBase.h"
#include "AkStreamingDefaults.h"
#include <AK/Tools/Common/AkPlatformFuncs.h>
#ifdef AK_SUPPORT_WCHAR
#include <wchar.h>
#endif //AK_SUPPORT_WCHAR
#include <stdio.h>

using namespace AK;
using namespace AK::StreamMgr;

//-------------------------------------------------------------------
// Defines.
//-------------------------------------------------------------------

#define AK_MINIMAL_THROUGHPUT	(1.f)    // 1 byte/second.
#define AK_CACHING_STREAM_MIN_BUFFER_SIZE (2048) //See AK_WORST_CASE_MIN_STREAM_BUFFER_SIZE in AkSrcFileBase.h

//-------------------------------------------------------------------
// CAkDeviceBase implementation.
//-------------------------------------------------------------------

// Device construction/destruction.
CAkDeviceBase::CAkDeviceBase( 
	IAkLowLevelIOHook *	in_pLowLevelHook
	)
: m_pLowLevelHook( in_pLowLevelHook )
, m_uMaxCachePinnedBytes(AkUInt32(-1))
, m_uCurrentCachePinnedData(0)
#ifndef AK_OPTIMIZED
, m_uNumActiveStreams( 0 )
, m_uBytesLowLevelThisInterval( 0 )
, m_uBytesThisInterval( 0 )
, m_uNumLowLevelRequests( 0 )
, m_uNumLowLevelRequestsCancelled( 0 )
, m_uBytesThisSession(0)
, m_uCacheBytesThisSession(0)
, m_bIsMonitoring( false )
, m_bIsNew( true )
#endif
{
}

CAkDeviceBase::~CAkDeviceBase( )
{
}

// Init.
AKRESULT CAkDeviceBase::Init( 
    const AkDeviceSettings &	in_settings,
    AkDeviceID					in_deviceID 
    )
{
    if ( 0 == in_settings.uGranularity )
    {
        AKASSERT( !"Invalid I/O granularity: must be non-zero" );
        return AK_InvalidParameter;
    }
    if ( in_settings.uIOMemorySize && 
         in_settings.fTargetAutoStmBufferLength < 0 )
    {
        AKASSERT( !"Invalid automatic stream average buffering value" );
        return AK_InvalidParameter;
    }
	if ( in_settings.uSchedulerTypeFlags & AK_SCHEDULER_DEFERRED_LINED_UP 
		&& ( in_settings.uMaxConcurrentIO < 1 || in_settings.uMaxConcurrentIO > 1024 ) )
    {
        AKASSERT( !"Invalid maximum number of concurrent I/O" );
        return AK_InvalidParameter;
    }

    m_uGranularity			= in_settings.uGranularity;
    m_fTargetAutoStmBufferLength  = in_settings.fTargetAutoStmBufferLength;
	m_uMaxConcurrentIO		= in_settings.uMaxConcurrentIO;
	
	m_deviceID				= in_deviceID;

	m_uMaxCachePinnedBytes = in_settings.uMaxCachePinnedBytes;

	if ( m_mgrMemIO.Init( in_settings, this ) != AK_Success )
	{
		AKASSERT( !"Failed creating IO memory manager." );
		return AK_Fail;
	}
	
	// Create I/O scheduler thread objects.
	return CAkIOThread::Init( in_settings.threadProperties );
}

// Destroy.
void CAkDeviceBase::Destroy()
{
	CAkIOThread::Term();

#ifndef AK_OPTIMIZED
    m_arStreamProfiles.Term();
#endif
	m_mgrMemIO.Term();

    AkDelete( CAkStreamMgr::GetObjPoolID(), this );
}

// Device ID. Virtual method defined in IAkDevice.
AkDeviceID CAkDeviceBase::GetDeviceID()
{
    return m_deviceID;
}

CAkStmTask * CAkDeviceBase::CreateStd(
	AkFileDesc *				in_pFileDesc,		// Low-level IO file descriptor.
	AkOpenMode					in_eOpenMode,       // Open mode (read, write, ...).
	IAkStdStream *&				out_pStream         // Returned interface to a standard stream.    
	)
{
	CAkStmTask * pNewStm = _CreateStd(
		in_pFileDesc,
		in_eOpenMode,
		out_pStream
		);

	if (pNewStm)
	{
		AddTask( pNewStm, m_listTasks );
	}

	return pNewStm;
}

CAkStmTask * CAkDeviceBase::CreateAuto(
	AkFileDesc *				in_pFileDesc,		// Low-level IO file descriptor.
	AkFileID					in_fileID,			// Application defined ID. Pass AK_INVALID_FILE_ID if unknown.
	const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
	AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
	IAkAutoStream *&            out_pStream         // Returned interface to an automatic stream.
	)
{
	CAkStmTask * pNewStm = _CreateAuto(
		in_pFileDesc,		
		in_fileID,			
		in_heuristics,      
		in_pBufferSettings, 
		out_pStream );

	if (pNewStm)
	{
		AddTask( pNewStm, m_listTasks );
	}

	return pNewStm;
}

CAkStmTask * CAkDeviceBase::CreateCachingStream(
	AkFileDesc *				in_pFileDesc,		// Low-level IO file descriptor.
	AkFileID					in_fileID,			// Application defined ID. Pass AK_INVALID_FILE_ID if unknown.
	AkUInt32					in_uNumBytesPrefetch,
	AkPriority					in_uPriority,
	IAkAutoStream *&            out_pStream         // Returned interface to an automatic stream.
	)
{
	AkAutoStmHeuristics heuristics;
	heuristics.uMinNumBuffers = 0; 
	heuristics.fThroughput = 0;//will be set below
	heuristics.uLoopStart = 0;
	heuristics.uLoopEnd = 0;
	heuristics.priority = in_uPriority;

	CAkStmTask * pTask = _CreateAuto(in_pFileDesc, in_fileID, heuristics, NULL, out_pStream);

	if (pTask)
	{
 		AKASSERT(out_pStream);
		pTask->SetIsCachingStream();
		pTask->SetCachingBufferSize( in_uNumBytesPrefetch );
		
		AddTask(pTask, m_listCachingTasks);
	}

	return pTask;
}

// Destroys all streams remaining to be destroyed.
bool CAkDeviceBase::ClearStreams()
{
	bool emptyTasks			= ClearTaskList( m_listTasks );
	bool emptyCachingTasks	= ClearTaskList( m_listCachingTasks );
	return emptyTasks && emptyCachingTasks;
}

bool CAkDeviceBase::ClearTaskList(TaskList& in_listTasks)
{
	TaskList::IteratorEx it = in_listTasks.BeginEx();
	while ( it != in_listTasks.End() )
	{
		CAkStmTask * pTask = (*it);
		if ( pTask->IsToBeDestroyed() )
		{
			if ( pTask->CanBeDestroyed() )
			{
				it = in_listTasks.Erase( it );
				pTask->InstantDestroy();
			}
			else
			{
				// Cannot be destroyed now (has I/O pending). Try again later.
				++it;
			}
		}
		else
		{
			// Set status as "error" and wait until client destroys us.
			pTask->Kill();
			++it;
		}
	}

	if ( in_listTasks.IsEmpty() )
	{
		in_listTasks.Term();
		return true;
	}
	return false;
}

// Called once when I/O thread starts.
void CAkDeviceBase::OnThreadStart()
{
	// Stamp time the first time.
    AKPLATFORM::PerformanceCounter( &m_time );
}

// Helper: adds a new task to the list.
// Sync: task list lock.
void CAkDeviceBase::AddTask( 
    CAkStmTask * in_pStmTask,
	TaskList& in_listToAddTo
    )
{
    AkAutoLock<CAkLock> gate( m_lockTasksList );
    
    in_listToAddTo.AddFirst( in_pStmTask );

#ifndef AK_OPTIMIZED
    // Compute and assign a new unique stream ID.
    in_pStmTask->SetStreamID(
		CAkStreamMgr::GetNewStreamID() // Gen stream ID.
        );
#endif
}

void CAkDeviceBase::UpdateCachingPriority(CAkAutoStmBase * in_pStream, AkPriority in_uNewPriority)
{
	AkPriority uOldPriority = in_pStream->Priority();

	if (uOldPriority != in_uNewPriority)
	{
		in_pStream->SetPriority(in_uNewPriority);

		// Restart all the tasks to re-evaluate the priorities.
			AkAutoLock<CAkLock> scheduling( m_lockTasksList );
			for ( TaskList::IteratorEx it = m_listCachingTasks.BeginEx(); it != m_listCachingTasks.End(); ++it )
			{
				CAkStmTask * pTask = (*it);
					pTask->StartCaching();
				}
			}

}

CAkStmTask * CAkDeviceBase::SchedulerFindNextCachingTask()
{
	CAkStmTask * pTaskToSchedule = NULL;

	AkAutoLock<CAkLock> scheduling( m_lockTasksList );

	AkUInt32 uCurrentCachePinned = 0;
	bool bStreamDestroyed = false;

	TaskList::IteratorEx it = m_listCachingTasks.BeginEx();
	while ( it != m_listCachingTasks.End() )
	{
		if ( (*it)->IsToBeDestroyed() && (*it)->CanBeDestroyed() )
		{
			CAkStmTask * pTaskToDestroy = (*it);
			it = m_listCachingTasks.Erase( it );
			pTaskToDestroy->InstantDestroy();
			bStreamDestroyed = true;
		}
		else
		{
			uCurrentCachePinned += (*it)->GetVirtualBufferingSize();
			++it;
		}
	}
	
	// Find the highest priority task that has been serviced the least recently.
	for (TaskList::IteratorEx it = m_listCachingTasks.BeginEx(); it != m_listCachingTasks.End(); ++it )
	{
		if ( bStreamDestroyed ) 
		{
			// A stream was destroyed.  Start any caching streams that may have been stopped 
			//	due to hitting the memory limit. We may be able to fit them in now.
			(*it)->StartCaching();
		}

		if ( (*it)->RequiresScheduling() )
		{
			// Choose task with highest priority to schedule
			if ( !pTaskToSchedule || (*it)->Priority() > pTaskToSchedule->Priority() )
			{
				pTaskToSchedule = (*it);
			}
		}
	}
	
	if ( pTaskToSchedule != NULL )
	{
		//Now check to see that if the amount of memory needed fits under our cache locked memory limit.
		AkUInt32 uMemNeededForTask = pTaskToSchedule->GetNominalBuffering() - pTaskToSchedule->GetVirtualBufferingSize();

		if ((uCurrentCachePinned + uMemNeededForTask) > m_uMaxCachePinnedBytes )
		{
			// Choose task with lowest priority to bump, if possible
			CAkStmTask * pTaskToBump;
			do
			{
				pTaskToBump = NULL;
				for (TaskList::IteratorEx it = m_listCachingTasks.BeginEx(); it != m_listCachingTasks.End(); ++it )
				{
					if ( (*it)->GetVirtualBufferingSize() > 0 &&
						(	( (*it)->Priority() < pTaskToSchedule->Priority() ) &&
							( !pTaskToBump || (*it)->Priority() < pTaskToBump->Priority() )
						 ) )
					{
						pTaskToBump = (*it);
					}
				}

				if (pTaskToBump)
				{
					uCurrentCachePinned -= pTaskToBump->StopCaching(uMemNeededForTask);
					if ( (uCurrentCachePinned + uMemNeededForTask) <= m_uMaxCachePinnedBytes )
						break; //we have enough free space
				}
			}
			while ( pTaskToBump != NULL );

			// If we could not free enough memory, do not schedule a new task 
			if ( (uCurrentCachePinned + uMemNeededForTask) > m_uMaxCachePinnedBytes  )
			{
				pTaskToSchedule->StopCaching(0);
				pTaskToSchedule = NULL;
			}
		}
	}

	m_uCurrentCachePinnedData = uCurrentCachePinned;

	return pTaskToSchedule;
}

////////////////////////////////////////

// Scheduler algorithm.
// Finds the next task for which an I/O request should be issued.
// Return: If a task is found, a valid pointer to a task is returned, as well
// as the operation's deadline (for low-level IO heuristics).
// Otherwise, returns NULL.
// Sync: 
// 1. Locks task list ("scheduler lock").
// 2. If it chooses a standard stream task, the stream becomes "I/O locked" before the scheduler lock is released.
CAkStmTask * CAkDeviceBase::SchedulerFindNextTask( 
	AkReal32 &		out_fOpDeadline		// Returned deadline for this transfer.
    )
{
    // Start scheduling.
    // ------------------------------------
	
	// Lock tasks list.
    AkAutoLock<CAkLock> scheduling( m_lockTasksList );

    // Stamp time.
    AKPLATFORM::PerformanceCounter( &m_time );

    // If m_bDoWaitMemoryChange, no automatic stream operation can be scheduled because memory is full
    // and will not be reassigned until someone calls NotifyMemChange().
    // Therefore, we only look for a pending standard stream (too bad if memory is freed in the meantime).
    if ( CannotScheduleAutoStreams() )
        return ScheduleStdStmOnly( out_fOpDeadline );

    TaskList::IteratorEx it = m_listTasks.BeginEx();
    CAkStmTask * pTask = NULL;
	AkReal32 fSmallestDeadline;
	bool bLeastBufferedTaskRequiresScheduling = false;
    
    // Get first valid task's values for comparison.
    while ( it != m_listTasks.End() )
    {
		// Verify that we can perform I/O on this one.
        if ( (*it)->IsToBeDestroyed() )
        {
			if ( (*it)->CanBeDestroyed() )
			{
				// Clean up.
				CAkStmTask * pTaskToDestroy = (*it);
	            it = m_listTasks.Erase( it );
				pTaskToDestroy->InstantDestroy();
	        }
			else 
			{
				// Not ready to be destroyed: wait until next turn.
				++it;
			}
        }
        else if ( !(*it)->ReadyForIO() )
            ++it;   // Skip this one.
        else
        {
            // Current iterator refers to a task that is not scheduled to be destroyed, and is ready for I/O. Proceed.
			// pTask and fSmallestDeadline are the current task and associated effective deadline chosen for I/O.
            pTask = (*it);
			fSmallestDeadline = pTask->EffectiveDeadline();
			bLeastBufferedTaskRequiresScheduling = pTask->RequiresScheduling();
			break;  
        }
    }    

    if ( !pTask )
    {
        // No task was ready for I/O. Leave.
        return SchedulerFindNextCachingTask();
    }

    // Find task with smallest effective deadline.
    // If a task has a deadline equal to 0, this means we are starving; user throughtput is greater than
    // low-level bandwidth. In that situation, starving streams are chosen according to their priority.
    // If more than one starving stream has the same priority, the scheduler chooses the one that has been 
    // waiting for I/O for the longest time.
    // Note 1: This scheduler does not have any idea of the actual low-level throughput, nor does it try to
    // know it. It just reacts to the status of its streams at a given moment.
    // Note 2: By choosing the highest priority stream only when we encounter starvation, we take the bet
    // that the transfer will complete before the user has time consuming its data. Therefore it remains 
    // possible that high priority streams starve.
    // Note 3: Automatic streams that just started are considered starving. They are chosen according to
    // their priority first, in a round robin fashion (starving mechanism).
    // Note 4: If starving mode lasts for a long time, low-priority streams will stop being chosen for I/O.
	// Note 5: Tasks that are actually signaled (RequireScheduling) have priority over other tasks. A task 
	// that is unsignaled may still have a smaller deadline than other tasks (because tasks must be double-
	// buffered at least). However, an unsignaled task will only be chosen if there are no signaled task.

    // Start with the following task.
    AKASSERT( pTask );
    ++it;

    while ( it != m_listTasks.End() )
    {
        // Verify that we can perform I/O on this one.
        if ( (*it)->IsToBeDestroyed() )
        {
			if ( (*it)->CanBeDestroyed() )
			{
				// Clean up.
				CAkStmTask * pTaskToDestroy = (*it);
	            it = m_listTasks.Erase( it );
				pTaskToDestroy->InstantDestroy();
	        }
			else 
			{
				// Not ready to be destroyed: wait until next turn.
				++it;
			}
        }
        else if ( (*it)->ReadyForIO() )
        {
			AkReal32 fDeadline = (*it)->EffectiveDeadline();

			if ( !bLeastBufferedTaskRequiresScheduling && (*it)->RequiresScheduling() )
			{
				// This is the first task that we run into which actually requires action from 
				// scheduler: pick it. 
				pTask = (*it);
				fSmallestDeadline = fDeadline;
				bLeastBufferedTaskRequiresScheduling = true;
			}
			else if ( !bLeastBufferedTaskRequiresScheduling || (*it)->RequiresScheduling() )
			{
				if ( fDeadline == 0 )
				{
					// Deadline is zero: starvation mode.
					// Choose task with highest priority among those that are starving.
					if ( (*it)->Priority() > pTask->Priority() || fSmallestDeadline > 0 )
					{
						pTask = (*it);
						fSmallestDeadline = fDeadline;
						bLeastBufferedTaskRequiresScheduling = pTask->RequiresScheduling();
					}
					else if ( (*it)->Priority() == pTask->Priority() )
					{
						// Same priority: choose the one that has waited the most.
						if ( (*it)->TimeSinceLastTransfer( GetTime() ) > pTask->TimeSinceLastTransfer( GetTime() ) )
						{
							pTask = (*it);
							fSmallestDeadline = fDeadline;
							bLeastBufferedTaskRequiresScheduling = pTask->RequiresScheduling();
						}
					}
				}
				else if ( fDeadline < fSmallestDeadline )
				{
					// Deadline is not zero: low-level has enough bandwidth. Just take the task with smallest deadline.
					// We take the bet that this transfer will have time to occur fast enough to properly service
					// the others on next pass.
					pTask = (*it);
					fSmallestDeadline = fDeadline;
					bLeastBufferedTaskRequiresScheduling = pTask->RequiresScheduling();
				}
			}
            ++it;
        }
        else
            ++it;   // Skip this task: it is not waiting for I/O.
	}

	out_fOpDeadline = fSmallestDeadline;

	if ( bLeastBufferedTaskRequiresScheduling )
	    return pTask;
	return SchedulerFindNextCachingTask();
}

// Scheduler algorithm: standard stream-only version.
// Finds next task among standard streams only (typically when there is no more memory).
// Note: standard streams that are ready for IO are always signaled.
CAkStmTask * CAkDeviceBase::ScheduleStdStmOnly(
	AkReal32 &	out_fOpDeadline	// Returned deadline for this transfer.
    )
{
    TaskList::IteratorEx it = m_listTasks.BeginEx();
    CAkStmTask * pTask = NULL;
    
    // Get first valid task's values for comparison.
    while ( it != m_listTasks.End() )
    {
        // Verify that we can perform I/O on this one.
        if ( (*it)->IsToBeDestroyed() )
        {
            if ( (*it)->CanBeDestroyed() )
			{
				// Clean up.
				CAkStmTask * pTaskToDestroy = (*it);
	            it = m_listTasks.Erase( it );
				pTaskToDestroy->InstantDestroy();
	        }
			else 
			{
				// Not ready to be destroyed: wait until next turn.
				++it;
			}
        }
        else if ( (*it)->StmType() == AK_StmTypeStandard &&
                    (*it)->ReadyForIO() )
        {
            // Current iterator refers to a standard stream task that is not scheduled to be destroyed, 
            // and that is pending. Proceed.
            pTask = (*it);
            break;
        }
        else
            ++it;
    }    

    if ( !pTask )
    {
        // No task was ready for I/O. Leave.
        return NULL;
    }

    // fSmallestDeadline is the smallest effective deadline found to date. Used to find the next task for I/O.
    AkReal32 fSmallestDeadline = pTask->EffectiveDeadline();
    
    // Find task with smallest effective deadline.
    // See note in SchedulerFindNextTask(). It is the same algorithm, except that automatic streams are excluded.
    
    // Start with the following task.
    AKASSERT( pTask );
    ++it;

    while ( it != m_listTasks.End() )
    {
        // Verify that we can perform I/O on this one.
        if ( (*it)->IsToBeDestroyed() )
        {
            if ( (*it)->CanBeDestroyed() )
			{
				// Clean up.
				CAkStmTask * pTaskToDestroy = (*it);
	            it = m_listTasks.Erase( it );
				pTaskToDestroy->InstantDestroy();
	        }
			else 
			{
				// Not ready to be destroyed: wait until next turn.
				++it;
			}
        }
        else if ( (*it)->StmType() == AK_StmTypeStandard &&
                    (*it)->ReadyForIO() )
        {
            AkReal32 fDeadline = (*it)->EffectiveDeadline(); 

            if ( fDeadline == 0 )
            {
                // Deadline is zero. Starvation mode: user throughput is greater than low-level bandwidth.
                // Choose task with highest priority among those that are starving.
                if ( (*it)->Priority() > pTask->Priority() || fSmallestDeadline > 0 )
                {
                    pTask = (*it);
                    fSmallestDeadline = fDeadline;
                }
                else if ( (*it)->Priority() == pTask->Priority() )
                {
                    // Same priority: choose the one that has waited the most.
                    if ( (*it)->TimeSinceLastTransfer( GetTime() ) > pTask->TimeSinceLastTransfer( GetTime() ) )
                    {
                        pTask = (*it);
                        fSmallestDeadline = fDeadline;
                    }
                }
            }
            else if ( fDeadline < fSmallestDeadline )
            {
                // Deadline is not zero: low-level has enough bandwidth. Just take the task with smallest deadline.
                // We take the bet that this transfer will have time to occur fast enough to properly service
                // the others.
                pTask = (*it);
                fSmallestDeadline = fDeadline;
            }
            ++it;
        }
        else
            ++it;   // Skip this task; not waiting for I/O.
    }

    AKASSERT( pTask );

	out_fOpDeadline = fSmallestDeadline;

    return pTask;

}

void CAkDeviceBase::ForceCleanup(
								 bool in_bKillLowestPriorityTask,				// True if the device should kill the task with lowest priority.
								 AkPriority in_priority							// Priority of the new task if applicable. Pass AK_MAX_PRIORITY to ignore.
								 )
{
	bool cachingTaskKilled = ForceTaskListCleanup( in_bKillLowestPriorityTask, in_priority, m_listCachingTasks );
	ForceTaskListCleanup( in_bKillLowestPriorityTask && !cachingTaskKilled, in_priority, m_listTasks );
}

// Forces the device to clean up dead tasks. 
bool CAkDeviceBase::ForceTaskListCleanup(
	bool in_bKillLowestPriorityTask,				// True if the device should kill the task with lowest priority.
	AkPriority in_priority,							// Priority of the new task if applicable. Pass AK_MAX_PRIORITY to ignore.
	TaskList& in_listTasks
	)
{
	AkAutoLock<CAkLock> scheduling( m_lockTasksList );

	CAkStmTask * pTaskToKill = NULL;
	TaskList::IteratorEx it = in_listTasks.BeginEx();
    while ( it != in_listTasks.End() )
    {
        // Cleanup while we're at it.
        if ( (*it)->IsToBeDestroyed() )
        {
            if ( (*it)->CanBeDestroyed() )
			{
				// Clean up.
				CAkStmTask * pTaskToDestroy = (*it);
	            it = in_listTasks.Erase( it );
				pTaskToDestroy->InstantDestroy();
	        }
			else
			{
				// Not ready to be destroyed: wait until next turn.
				++it;
			}			
        }
		// Otherwise, check if it is a candidate to be killed.
		else if ( in_bKillLowestPriorityTask
				&& ( !pTaskToKill || (*it)->Priority() < pTaskToKill->Priority() )
				&& (*it)->Priority() < in_priority
				&& (*it)->ReadyForIO() )
        {
            // Current iterator refers to a standard stream task that is not scheduled to be destroyed, 
            // and that is pending. Proceed.
            pTaskToKill = (*it);
			++it;
        }
        else
            ++it;
    }

	// Kill the task if any.
	if ( pTaskToKill )
	{
		pTaskToKill->Kill();
		return true;
	}

	return false;
}

// Cache management.
void CAkDeviceBase::FlushCache()
{
	AkAutoLock<CAkIOThread> lock( *this );
	m_mgrMemIO.FlushCache();
}

// Perform a direct transfer from the cache, from client thread.
// Sync: task must be locked. Device is locked herein.
bool CAkDeviceBase::ExecuteCachedTransfer( CAkStmTask * in_pTask )
{
	AKASSERT( in_pTask != NULL );

	AkAutoLock<CAkIOThread> lock(*this);

	// Get info for IO.
	AkFileDesc * pFileDesc;
	CAkLowLevelTransfer * pNewLowLevelXfer;
	bool bTrasferExists;
	CAkStmMemView * pMemView = in_pTask->PrepareTransfer( 
		pFileDesc, 
		pNewLowLevelXfer,
		bTrasferExists,
		true );
	
	AKASSERT( !pNewLowLevelXfer || !"Asked for cached transfer only" );
	AKASSERT( !pMemView || !bTrasferExists || !"PrepareTransfer() should only return mem views that do not have pending transfers.");

	// Update task after transfer.  Returns true if buffer was added
	return in_pTask->Update(pMemView, AK_Success, false);
}

// Device Profile Ex interface.
// --------------------------------------------------------
#ifndef AK_OPTIMIZED

void CAkDeviceBase::OnProfileStart()
{
#if defined(WWISE_AUTHORING)
	m_lockTasksList.Lock();
#endif
}

void CAkDeviceBase::OnProfileEnd()
{
#if defined(WWISE_AUTHORING)
	m_lockTasksList.Unlock();
#endif
}        

// Caps/desc.
void CAkDeviceBase::GetDesc( 
    AkDeviceDesc & out_deviceDesc 
    )
{
    m_pLowLevelHook->GetDeviceDesc( out_deviceDesc );
}

void CAkDeviceBase::GetData(
    AkDeviceData &  out_deviceData
    )
{
	AkAutoLock<CAkIOThread> lock(*this);

	m_mgrMemIO.GetProfilingData( m_uGranularity, out_deviceData );
	out_deviceData.deviceID = m_deviceID;
	out_deviceData.uGranularity = m_uGranularity;
	out_deviceData.uNumActiveStreams = m_uNumActiveStreams;
	out_deviceData.uTotalBytesTransferred = m_uBytesThisInterval;
	out_deviceData.uLowLevelBytesTransferred = m_uBytesLowLevelThisInterval;
	
	m_uBytesThisSession += m_uBytesThisInterval;
	AKASSERT( m_uBytesThisInterval >= m_uBytesLowLevelThisInterval );
	m_uCacheBytesThisSession += (m_uBytesThisInterval - m_uBytesLowLevelThisInterval);
	AKASSERT( m_uCacheBytesThisSession <= m_uBytesThisSession );

	out_deviceData.fAvgCacheEfficiency = 0.f;
	if (m_uBytesThisSession > 0)
		out_deviceData.fAvgCacheEfficiency = ((AkReal32)m_uCacheBytesThisSession / (AkReal32)m_uBytesThisSession) * 100.f;
	
	AKASSERT(out_deviceData.fAvgCacheEfficiency >= 0.f && out_deviceData.fAvgCacheEfficiency <= 100.f);

	out_deviceData.uNumLowLevelRequestsCompleted = m_uNumLowLevelRequests;
	out_deviceData.uNumLowLevelRequestsCancelled = m_uNumLowLevelRequestsCancelled;
	out_deviceData.uNumLowLevelRequestsPending = AkMin(m_uMaxConcurrentIO,GetNumConcurrentIO());
	out_deviceData.uCustomParam = m_pLowLevelHook->GetDeviceData();
	out_deviceData.uCachePinnedBytes = m_uCurrentCachePinnedData;
	
	m_uBytesLowLevelThisInterval = 0;
	m_uBytesThisInterval = 0;
	m_uNumLowLevelRequests = 0;
	m_uNumLowLevelRequestsCancelled = 0;
}

bool CAkDeviceBase::IsNew( )
{
    return m_bIsNew;
}

void CAkDeviceBase::ClearNew( )
{
    m_bIsNew = false;
}


AKRESULT CAkDeviceBase::StartMonitoring( )
{
    m_bIsMonitoring = true;
	// Reset transfer statistics.
	m_uBytesLowLevelThisInterval	= 0;
	m_uBytesThisInterval = 0;
	m_uNumLowLevelRequests = 0;
	m_uNumLowLevelRequestsCancelled = 0;

	m_uBytesThisSession = 0;
	m_uCacheBytesThisSession = 0;

    return AK_Success;
}

void CAkDeviceBase::StopMonitoring( )
{
    m_bIsMonitoring = false;
}

// Stream profiling: GetNumStreams.
// Clears profiling array.
// Inspects all streams. 
// Grants permission to destroy if scheduled for destruction and AND not new.
// Copies all stream profile interfaces into its array, that will be accessed
// by IAkStreamProfile methods.
AkUInt32 CAkDeviceBase::GetNumStreams()
{
	m_arStreamProfiles.RemoveAll();

	m_uNumActiveStreams = 0;

	AkAutoLock<CAkLock> gate( m_lockTasksList );

	CountStreamsInTaskList(m_listTasks);
	CountStreamsInTaskList(m_listCachingTasks);

	return m_arStreamProfiles.Length();
}

void CAkDeviceBase::CountStreamsInTaskList(TaskList& in_listTasks)
{
    TaskList::Iterator it = in_listTasks.Begin();
    while ( it != in_listTasks.End() )
    {
        // Profiler will let the device destroy this stream if it is not "new" (was already
		// read). If it is ready to be destroyed and not new, it could be because the file could not 
		// be opened, so the client will have destroyed the stream. In such a case the profiler allow  
		if ( (*it)->ProfileIsToBeDestroyed() 
			&& ( !(*it)->IsProfileNew() 
				|| !(*it)->IsProfileReady() ) )
		{
			(*it)->ProfileAllowDestruction();
		}
        else
        {
            // Copy into profiler list.
            m_arStreamProfiles.AddLast( (*it)->GetStreamProfile() );
			// Also, increment the number of active streams for device profiling.
			m_uNumActiveStreams += ( (*it)->WasActive() ) ? 1 : 0;
        }

        ++it;
    }
    
}

// Note. The following functions refer to streams by index, which must honor the call to GetNumStreams().
AK::IAkStreamProfile * CAkDeviceBase::GetStreamProfile( 
    AkUInt32    in_uStreamIndex             // [0,numStreams[
    )
{
    // Get stream profile and return.
    return m_arStreamProfiles[in_uStreamIndex];
}
#endif


//-----------------------------------------------------------------------------
//
// Stream objects implementation.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name: class CAkStmTask
// Desc: Base implementation common to streams. Defines the interface used by
//       the device scheduler.
//-----------------------------------------------------------------------------
CAkStmTask::CAkStmTask()
: m_pDeferredOpenData( NULL )
, m_pFileDesc( NULL )
, m_pszStreamName( NULL )
#ifndef AK_OPTIMIZED
//Not set m_ulStreamID;
, m_uBytesTransfered( 0 )
#endif
, m_bHasReachedEof( false )
, m_bIsToBeDestroyed( false )
, m_bIsFileOpen( false )
, m_bRequiresScheduling( false )
, m_bIsCachingStream( false )
, m_bIsReadyForIO( false )
#ifndef AK_OPTIMIZED
, m_bIsNew( true )
, m_bIsProfileDestructionAllowed( false )
, m_bWasActive( false )
, m_bCanClearActiveProfile( false )
#endif
{
}
CAkStmTask::~CAkStmTask()
{
	// Cleanup in Low-Level IO.
    AKASSERT( m_pDevice != NULL );
	if ( m_bIsFileOpen )
	{
		AKASSERT( m_pFileDesc );
	    m_pDevice->GetLowLevelHook()->Close( *m_pFileDesc );
	}

	FreeDeferredOpenData();

	if ( m_pszStreamName )
        AkFree( CAkStreamMgr::GetObjPoolID(), m_pszStreamName );

	if ( m_pFileDesc )
		AkFree( CAkStreamMgr::GetObjPoolID(), m_pFileDesc );
}

AKRESULT CAkStmTask::SetDeferredFileOpen(
	AkFileDesc *				in_pFileDesc,		// Low-level IO file descriptor.
	const AkOSChar*				in_pszFileName,		// File name.
	AkFileSystemFlags *			in_pFSFlags,		// File system flags (can be NULL).
	AkOpenMode					in_eOpenMode		// Open mode.
	)
{
	AKASSERT( !m_pDeferredOpenData );
	m_bIsFileOpen = false;	// We try to set a deferred open command: this means this file was not opened yet.

	AKASSERT( !m_pFileDesc );
	m_pFileDesc = in_pFileDesc;

	m_pDeferredOpenData = AkDeferredOpenData::Create( in_pszFileName, in_pFSFlags, in_eOpenMode );
	return ( m_pDeferredOpenData ) ? AK_Success : AK_Fail;
}

AKRESULT CAkStmTask::SetDeferredFileOpen(
	AkFileDesc *				in_pFileDesc,		// Low-level IO file descriptor.
	AkFileID					in_fileID,			// File ID.
	AkFileSystemFlags *			in_pFSFlags,		// File system flags (can be NULL).
	AkOpenMode					in_eOpenMode		// Open mode.
	)
{
	AKASSERT( !m_pDeferredOpenData );
	m_bIsFileOpen = false;	// We try to set a deferred open command: this mean this file was not opened yet.

	AKASSERT( !m_pFileDesc );
	m_pFileDesc = in_pFileDesc;

	m_pDeferredOpenData = AkDeferredOpenData::Create( in_fileID, in_pFSFlags, in_eOpenMode);			
	return ( m_pDeferredOpenData ) ? AK_Success : AK_Fail;
}

void CAkStmTask::FreeDeferredOpenData()
{
	if ( m_pDeferredOpenData )
	{
		m_pDeferredOpenData->Destroy();
		m_pDeferredOpenData = NULL;
	}
}

AKRESULT CAkStmTask::EnsureFileIsOpen()
{
	// Resolve pending file open command if it exists.
	if ( m_pDeferredOpenData && !m_bIsToBeDestroyed )
	{
		AKRESULT eResult = m_pDeferredOpenData->Execute(*m_pFileDesc);
		if (eResult == AK_Success)
			OnFileDeferredOpen();

		// Free m_pDeferredOpenData. No need to lock: this is always called from the I/O thread,
		// as is InstantDestroy().
		FreeDeferredOpenData();
		
		return eResult;
	}

	// Already open.
	return AK_Success;
}

#ifndef AK_OPTIMIZED
// Profiling: Get stream information. This information should be queried only once, since it is unlikely to change.
void CAkStmTask::GetStreamRecord( 
    AkStreamRecord & out_streamRecord
    )
{
    out_streamRecord.deviceID = m_pDevice->GetDeviceID( );
    if ( m_pszStreamName != NULL )
    {
        AK_OSCHAR_TO_UTF16( out_streamRecord.szStreamName, m_pszStreamName, AK_MONITOR_STREAMNAME_MAXLENGTH );    
		out_streamRecord.uStringSize = (AkUInt32)AKPLATFORM::AkUtf16StrLen( out_streamRecord.szStreamName ) + 1;
        out_streamRecord.szStreamName[AK_MONITOR_STREAMNAME_MAXLENGTH-1] = 0;
    }
    else
    {
        out_streamRecord.uStringSize = 0;    
        out_streamRecord.szStreamName[0] = 0;
    }
    out_streamRecord.uFileSize = m_pFileDesc->iFileSize;
    out_streamRecord.uStreamID = m_uStreamID;
	out_streamRecord.uCustomParamSize = m_pFileDesc->uCustomParamSize;
	out_streamRecord.uCustomParam = (AkUInt32)(AkUIntPtr)m_pFileDesc->pCustomParam;
	out_streamRecord.bIsAutoStream = m_bIsAutoStm;
	out_streamRecord.bIsCachingStream = IsCachingStream();
}
#endif

//-----------------------------------------------------------------------------
// Name: class CAkStdStmBase
// Desc: Standard stream base implementation.
//-----------------------------------------------------------------------------

CAkStdStmBase::CAkStdStmBase()
: m_memBlock( NULL )
, m_uTotalScheduledSize( 0 )
, m_eStmStatus( AK_StmStatusIdle )
, m_bIsOpComplete( true )
{
	m_bIsAutoStm = false;
	m_bIsWriteOp = false;
}

CAkStdStmBase::~CAkStdStmBase( )
{
	// If the stream kept asking to be scheduled, now it is time to stop.
    if ( m_bRequiresScheduling )
        m_pDevice->StdSemDecr();
}

// Init.
// Sync: None.
AKRESULT CAkStdStmBase::Init(
    CAkDeviceBase *     in_pDevice,         // Owner device.
    AkFileDesc *		in_pFileDesc,       // File descriptor.
    AkOpenMode								// Open mode.
    )
{
    AKASSERT( in_pDevice != NULL );

    m_pDevice	= in_pDevice;

    // Store file descriptor.
    if ( in_pFileDesc->iFileSize < 0 )
    {
		SetToBeDestroyed();
        AKASSERT( !"Invalid file size" );
        return AK_InvalidParameter;
    }
    
	AKASSERT( m_pszStreamName == NULL );

	const AkUInt32 uLLBlockSize = in_pDevice->GetLowLevelHook()->GetBlockSize( *in_pFileDesc );
    if ( !uLLBlockSize
		|| uLLBlockSize > in_pDevice->GetGranularity()
		|| ( in_pDevice->GetGranularity() % uLLBlockSize ) > 0 )
    {
		SetToBeDestroyed();
		AK_MONITOR_ERROR( AK::Monitor::ErrorCode_IncompatibleIOSettings );
		AKASSERT( !"Invalid Low-Level I/O block size. Must be >= 1 && <= Granularity and a multiple of the granularity" );
        return AK_Fail;
    }
	m_uLLBlockSize = uLLBlockSize;

	// IMPORTANT: Do not set m_pFileDesc here.
	AKASSERT( !m_pFileDesc );

	return AK_Success;
}

//-----------------------------------------------------------------------------
// IAkStdStream interface.
//-----------------------------------------------------------------------------

// Stream info access.
// Sync: None.
void CAkStdStmBase::GetInfo(
    AkStreamInfo & out_info       // Returned stream info.
    )
{
    AKASSERT( m_pDevice != NULL );
    out_info.deviceID	= m_pDevice->GetDeviceID();
    out_info.pszName	= m_pszStreamName;
    out_info.uSize		= m_pFileDesc->iFileSize;
	out_info.bIsOpen	= m_bIsFileOpen;
}

// Name the stream (appears in Wwise profiler).
// Sync: None
AKRESULT CAkStdStmBase::SetStreamName(
    const AkOSChar * in_pszStreamName    // Stream name.
    )
{
    if ( m_pszStreamName != NULL )
        AkFree( CAkStreamMgr::GetObjPoolID(), m_pszStreamName );

    if ( in_pszStreamName != NULL )
    {
        // Allocate string buffer for user defined stream name.
		size_t uStrLen = AKPLATFORM::OsStrLen( in_pszStreamName ) + 1;
        m_pszStreamName = (AkOSChar*)AkAlloc( CAkStreamMgr::GetObjPoolID(), uStrLen * sizeof(AkOSChar) );
        if ( m_pszStreamName == NULL )
            return AK_InsufficientMemory;

        // Copy.
		AKPLATFORM::SafeStrCpy( m_pszStreamName, in_pszStreamName, uStrLen );
    }
    return AK_Success;
}

// Get low-level block size for this stream.
// Returns block size for optimal/unbuffered IO.
AkUInt32 CAkStdStmBase::GetBlockSize()
{
    return m_uLLBlockSize;
}

// Get stream position.
// Sync: None. 
// Users should not call this when pending.
AkUInt64 CAkStdStmBase::GetPosition( 
    bool * out_pbEndOfStream   // Input streams only. Can be NULL.
    )   
{
    AKASSERT( m_eStmStatus != AK_StmStatusPending ||
              !"Inaccurate stream position when operation is pending" );
    if ( out_pbEndOfStream != NULL )
    {
        *out_pbEndOfStream = m_bHasReachedEof;
    }
    return GetCurUserPosition();
}

// Operations.
// ------------------------------------------

// Set stream position. Modifies position of next read/write.
// Sync: 
// Fails if an operation is pending.
AKRESULT CAkStdStmBase::SetPosition(
    AkInt64         in_iMoveOffset,     // Seek offset.
    AkMoveMethod    in_eMoveMethod,     // Seek method, from beginning, end or current file position.
    AkInt64 *       out_piRealOffset    // Actual seek offset may differ from expected value when unbuffered IO. 
                                        // In that case, floors to sector boundary. Pass NULL if don't care.
    )
{
    if ( out_piRealOffset != NULL )
    {
        *out_piRealOffset = 0;
    }

    // Safe status.
    if ( m_eStmStatus == AK_StmStatusPending )
    {
        AKASSERT( !"Trying to change stream position while standard IO is pending" );
        return AK_Fail;
    }

    // Compute position.
    AkInt64 iPosition;
    if ( in_eMoveMethod == AK_MoveBegin )
    {
        iPosition = in_iMoveOffset;
    }
    else if ( in_eMoveMethod == AK_MoveCurrent )
    {
        iPosition = GetCurUserPosition() + in_iMoveOffset;
    }
    else if ( in_eMoveMethod == AK_MoveEnd )
    {
        iPosition = m_pFileDesc->iFileSize + in_iMoveOffset;
    }
    else
    {
        AKASSERT( !"Invalid move method" );
        return AK_InvalidParameter;
    }

    if ( iPosition < 0 )
    {
        AKASSERT( !"Trying to move the file pointer before the beginning of the file" );
        return AK_InvalidParameter;
    }

    // Round offset to block size.
    if ( iPosition % m_uLLBlockSize != 0 )
    {
        // Snap to lower boundary.
        iPosition -= ( iPosition % m_uLLBlockSize );
        AKASSERT( iPosition >= 0 );
    }

    // Set real offset if argument specified.
    if ( out_piRealOffset != NULL )
    {
        switch ( in_eMoveMethod )
        {
        case AK_MoveBegin:
            *out_piRealOffset = iPosition;
            break;
        case AK_MoveCurrent:
            *out_piRealOffset = iPosition - GetCurUserPosition();
            break;
        case AK_MoveEnd:
            *out_piRealOffset = iPosition - m_pFileDesc->iFileSize;
            break;
        default:
            AKASSERT( !"Invalid move method" );
            return AK_Fail;
        }
    }

    // Update position if it changed.
    // Set new file position.
    SetCurUserPosition( iPosition );
    return AK_Success;
}

// Read.
// Sync: Returns if task pending. Status change.
AKRESULT CAkStdStmBase::Read(
    void *          in_pBuffer,         // User buffer address. 
    AkUInt32        in_uReqSize,        // Requested read size.
    bool            in_bWait,           // Block until operation is complete.
    AkPriority      in_priority,        // Heuristic: operation priority.
    AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
    AkUInt32 &      out_uSize           // Size actually read.
    )
{
    return ExecuteOp( false,// (Read)
		in_pBuffer,         // User buffer address. 
		in_uReqSize,        // Requested write size. 
		in_bWait,           // Block until operation is complete.
		in_priority,        // Heuristic: operation priority.
		in_fDeadline,       // Heuristic: operation deadline (s).
		out_uSize );        // Size actually written.
}

// Write.
// Sync: Returns if task pending. Changes status.
AKRESULT CAkStdStmBase::Write(
    void *          in_pBuffer,         // User buffer address. 
    AkUInt32        in_uReqSize,        // Requested write size. 
    bool            in_bWait,           // Block until operation is complete.
    AkPriority      in_priority,        // Heuristic: operation priority.
    AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
    AkUInt32 &      out_uSize           // Size actually written.
    )
{
    return ExecuteOp( true,	// (Write)
		in_pBuffer,         // User buffer address. 
		in_uReqSize,        // Requested write size. 
		in_bWait,           // Block until operation is complete.
		in_priority,        // Heuristic: operation priority.
		in_fDeadline,       // Heuristic: operation deadline (s).
		out_uSize );        // Size actually written.
}

// Execute Operation (either Read or Write).
AKRESULT CAkStdStmBase::ExecuteOp(
	bool			in_bWrite,			// Read (false) or Write (true).
	void *          in_pBuffer,         // User buffer address. 
    AkUInt32        in_uReqSize,        // Requested write size. 
    bool            in_bWait,           // Block until operation is complete.
    AkPriority      in_priority,        // Heuristic: operation priority.
    AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
    AkUInt32 &      out_uSize           // Size actually written.
	)
{
    out_uSize					= 0;
	m_uTotalScheduledSize		= 0;
	m_bIsOpComplete				= false;
	m_bIsWriteOp				= in_bWrite;
    m_priority					= in_priority;
    m_fDeadline					= in_fDeadline;
	// Make our mem block represent the user provided buffer. The file position stored in m_memBlock is the current
	// user position, that is, the position where this operation should start.
	m_memBlock.pData			= in_pBuffer;
	m_memBlock.uAvailableSize	= in_uReqSize;

    // Check requested size.
    if ( in_pBuffer == NULL )
    {
        AKASSERT( !"Invalid buffer" );
        return AK_InvalidParameter;
    }

    // Check heuristics.
    if ( in_priority < AK_MIN_PRIORITY ||
         in_priority > AK_MAX_PRIORITY ||
         in_fDeadline < 0 )
    {
        AKASSERT( !"Invalid heuristics" );
        return AK_InvalidParameter;
    }

    // Check status.
    if ( m_eStmStatus == AK_StmStatusPending 
		|| m_eStmStatus == AK_StmStatusError )
    {
        return AK_Fail;
    }

    // Verify with block size.
	if (!in_bWrite)
	{
		if ( in_uReqSize % m_uLLBlockSize != 0 )
		{
			AKASSERT( !"Requested size incompatible with Low-Level block size" );
			return AK_Fail;
		}

		if ( m_bIsFileOpen )
		{
			bool bEof;	// unused
			in_uReqSize = ClampRequestSizeToEof( GetCurUserPosition(), in_uReqSize, bEof );
		}
	}

	// Leave right away if requested size turns out to be 0 (only for Read).
	if ( 0 == in_uReqSize )
	{
		AkAutoLock<CAkLock> status( m_lockStatus );
		SetStatus( AK_StmStatusCompleted );
		out_uSize = 0;
		return AK_Success;
	}

    // Reset time.
	AKPLATFORM::PerformanceCounter( &m_iIOStartTime );
    
    // If blocking, register this thread as blocked.
	AKRESULT eResult;
    if ( in_bWait )
    {
		m_lockStatus.Lock();

		SetBlockedStatus();

	    // Set Status. Notify device sheduler.
		SetStatus( AK_StmStatusPending );
		m_lockStatus.Unlock();
		
	    // Wait for the blocking event.
		m_pDevice->WaitForIOCompletion( this );

		eResult = ( AK_StmStatusCompleted == m_eStmStatus ) ? AK_Success : AK_Fail;
    }
    else
    {
    	// Set Status. Notify device sheduler.
    	AkAutoLock<CAkLock> status( m_lockStatus );

		SetStatus( AK_StmStatusPending );
		eResult = AK_Success;
    }
    	
    out_uSize = in_uReqSize;

    return eResult;
}

// Get data and size.
// Returns address of data. No check for pending I/O.
// Sync: None. Always accurate when I/O not pending.
void * CAkStdStmBase::GetData( 
    AkUInt32 & out_uActualSize   // Size actually read or written.
    )
{
	// Note: m_memBlock.uAvailableSize maps the user provided memory, but was clamped to the end of file when 
	// the operation completed.
    out_uActualSize = m_memBlock.uAvailableSize;
    return m_memBlock.pData;
}
// Info access.
// Sync: None. Status query.
AkStmStatus CAkStdStmBase::GetStatus()
{
    return m_eStmStatus;
}

//-----------------------------------------------------------------------------
// CAkStmTask virtual methods implementation.
//-----------------------------------------------------------------------------

// Compute task's deadline for next operation.
// Sync: None: if m_uActualSize is changed in the meantime, the scheduler could take a suboptimal decision.
AkReal32 CAkStdStmBase::EffectiveDeadline()
{
	AkUInt32 uGranularity = m_pDevice->GetGranularity();
	AkUInt32 uNumTransfersRemaining = ( m_memBlock.uAvailableSize - m_uTotalScheduledSize + uGranularity - 1 ) / uGranularity;
	AKASSERT( uNumTransfersRemaining > 0 );
	AkReal32 fDeadline = ( m_fDeadline / uNumTransfersRemaining ) - AKPLATFORM::Elapsed( m_pDevice->GetTime(), m_iIOStartTime );
    return ( fDeadline > 0 ? fDeadline : 0 );
}

// Add a new streaming memory view (or content of a memory view) to this stream after a transfer.
// If it ends up not being used, it is disposed of. Otherwise it's status is set to Ready.
// All logical transfers must end up here, even if they were cancelled.
// Sync: Status must be locked prior to calling this function. 
void CAkStdStmBase::AddMemView( 
	CAkStmMemView * in_pMemView,		// Transfer-mode memory view to resolve.
	bool			in_bStoreData		// Store data in stream object only if true.
	)
{
	AKASSERT( in_pMemView );
	
	AkUInt32 uTransferSize = 0;

	if ( in_bStoreData
		&& !m_bIsToBeDestroyed 
		&& m_eStmStatus != AK_StmStatusError )
    {
		m_bHasReachedEof = false;

		// Deduce transfer size.
		m_bIsOpComplete = true;
		uTransferSize = in_pMemView->Size();
		if ( uTransferSize > m_pDevice->GetGranularity() )
		{
			uTransferSize = m_pDevice->GetGranularity();
			m_bIsOpComplete = false;
		}

		// Check EOF. in_pMemView->EndPosition() may overshoot file end position because it maps the 
		// user-provided memory.
		if ( !m_bIsWriteOp
			&& uTransferSize >= ( FileSize() - in_pMemView->StartPosition() ) )
		{
			uTransferSize = (AkUInt32)( FileSize() - in_pMemView->StartPosition() );
			m_bHasReachedEof = true;
			
			// Operation is complete, and we reached the end of file in read mode. Adjust available size for user.
			m_bIsOpComplete = true;
			AKASSERT( in_pMemView->EndPosition() >= FileSize() );
			AkUInt32 uFileOvershoot = (AkUInt32)( in_pMemView->EndPosition() - FileSize() );
			m_memBlock.uAvailableSize -= uFileOvershoot;
		}
		
		AKASSERT( uTransferSize <= m_memBlock.uAvailableSize
				&& ( m_bIsOpComplete || !m_bHasReachedEof ) );
	}
	// else 
	// Stream was either scheduled to be destroyed while I/O was occurring, stopped, or 
	// its position was set dirty while I/O was occuring. 

	// Once transfers are completed, standard streams don't keep data refs. Dispose of it now.
	{
		AkAutoLock<CAkIOThread> lock( *m_pDevice );

// Profiling.
#ifndef AK_OPTIMIZED
		m_uBytesTransfered += uTransferSize;

		AKASSERT( in_pMemView->Status() != CAkStmMemView::TransferStatus_Ready 
				|| !"Standard streams cannot use cache data" );
		if ( m_pDevice->IsMonitoring() )
			m_pDevice->PushTransferStatistics( uTransferSize, true );
#endif

		m_pDevice->DestroyMemView( &m_memBlock, in_pMemView );
	}
}

// Update task's status after transfer.
// Sync: Status must be locked prior to calling this function.
void CAkStdStmBase::UpdateTaskStatus(
	AKRESULT	in_eIOResult			// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
	)
{
    // Compute status.
    if ( in_eIOResult == AK_Fail )
    {
		// Change Status and update semaphore count.
		SetStatus( AK_StmStatusError );
    }
    else
    {
        if ( m_bIsOpComplete )
        {
			// Update client position.
			// Note: m_memBlock.uAvailableSize maps the user provided memory, but was clamped to the end of file when 
			// the operation completed.
			SetCurUserPosition( GetCurUserPosition() + m_memBlock.uAvailableSize );

			// Change Status and update semaphore count.
			SetStatus( AK_StmStatusCompleted );
        }
        // else Still pending: do not change status.
    }

    // Release the client thread if blocking I/O.
    if ( IsBlocked() 
		&& m_eStmStatus != AK_StmStatusPending 
		&& m_eStmStatus != AK_StmStatusIdle )
    {
		m_pDevice->SignalIOCompleted( this );
    }
}

void CAkStdStmBase::Kill()
{
	AkAutoLock<CAkLock> updateStatus( m_lockStatus );
	UpdateTaskStatus( AK_Fail );
}

//-----------------------------------------------------------------------------
// Profiling.
//-----------------------------------------------------------------------------
#ifndef AK_OPTIMIZED
// Get stream data.
void CAkStdStmBase::GetStreamData(
    AkStreamData & out_streamData
    )
{
	AkAutoLock<CAkLock> stmBufferGate(m_lockStatus);

    out_streamData.uStreamID = m_uStreamID;
    // Note. Priority appearing in profiler will be that which was used in last operation. 
    out_streamData.uPriority = m_priority;
    out_streamData.uTargetBufferingSize = 0;
    out_streamData.uVirtualBufferingSize = 0;
    out_streamData.uFilePosition = GetCurUserPosition();
    out_streamData.uNumBytesTransfered = m_uBytesTransfered;
	out_streamData.uNumBytesTransferedLowLevel = m_uBytesTransfered;
    m_uBytesTransfered = 0;    // Reset.
	out_streamData.uMemoryReferenced = 0;
	out_streamData.uBufferedSize = 0;
	out_streamData.fEstimatedThroughput = ( m_fDeadline > 0 ) ? m_memBlock.uAvailableSize / m_fDeadline : 0.f;
	out_streamData.bActive = m_bWasActive;
	if ( m_bCanClearActiveProfile )
	{
		m_bWasActive = false;
	}
}

// Signals that stream can be destroyed.
void CAkStdStmBase::ProfileAllowDestruction()
{
    AKASSERT( m_bIsToBeDestroyed );
    m_bIsProfileDestructionAllowed = true;
	AkAutoLock<CAkLock> statusChange( m_lockStatus );
	SetStatus( AK_StmStatusCancelled );
 }
#endif

//-----------------------------------------------------------------------------
// Helpers.
//-----------------------------------------------------------------------------
// Set task's status.
// Sync: Status must be locked.
void CAkStdStmBase::SetStatus( 
    AkStmStatus in_eStatus 
    )
{
	m_eStmStatus = in_eStatus;

	// Update semaphore.
	if ( IsToBeDestroyed() && CanBeDestroyed() )
	{
		// Requires clean up.
		if ( !m_bRequiresScheduling )
        {
            // Signal IO thread for clean up.
			m_bRequiresScheduling = true;
#ifndef AK_OPTIMIZED
			m_bWasActive = true;
			m_bCanClearActiveProfile = false;
#endif
            m_pDevice->StdSemIncr();
        }
	}
    else
    {
		if ( AK_StmStatusPending == in_eStatus )
		{
			// Requires IO transfer.
			AKASSERT( !IsToBeDestroyed() || !"Cannot call SetStatus(Pending) after stream was scheduled for termination" );
			SetReadyForIO( true );
			if ( !m_bRequiresScheduling )
			{
				m_bRequiresScheduling = true;
#ifndef AK_OPTIMIZED
				m_bWasActive = true;
				m_bCanClearActiveProfile = false;
#endif
				m_pDevice->StdSemIncr();
			}
		}
		else
		{
			// Does not require IO transfer.
			SetReadyForIO( false );
			if ( m_bRequiresScheduling )
			{
				m_bRequiresScheduling = false;
				m_pDevice->StdSemDecr();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Name: class CAkAutoStmBase
// Desc: Automatic stream base implementation.
//-----------------------------------------------------------------------------

CAkAutoStmBase::CAkAutoStmBase()
: m_fileID( AK_INVALID_FILE_ID )
, m_uVirtualBufferingSize( 0 )
#ifndef AK_OPTIMIZED
, m_uBytesTransferedLowLevel( 0 )
#endif
, m_uNextToGrant( 0 )
, m_bIsRunning( false )
, m_bIOError( false )
, m_bCachingReady( false )
{
	m_bIsAutoStm = true;
	m_bIsWriteOp = false;
}

CAkAutoStmBase::~CAkAutoStmBase( )
{
	// If the stream kept asking to be scheduled, now it is time to stop.
    if ( m_bRequiresScheduling )
        m_pDevice->AutoSemDecr();
}

// Init.
AKRESULT CAkAutoStmBase::Init( 
    CAkDeviceBase *             in_pDevice,         // Owner device.
	AkFileDesc *				in_pFileDesc,       // File descriptor.
	AkFileID					in_fileID,			// Application defined ID. Pass AK_INVALID_FILE_ID if unknown.
    const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
    AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings.
    AkUInt32                    in_uGranularity     // Device's I/O granularity.
    )
{
    AKASSERT( in_pDevice != NULL );
    AKASSERT( in_heuristics.priority >= AK_MIN_PRIORITY &&
              in_heuristics.priority <= AK_MAX_PRIORITY );

	m_pDevice           = in_pDevice;

    if ( in_pFileDesc->iFileSize < 0 )
    {
		SetToBeDestroyed();
        AKASSERT( !"Invalid file size" );
        return AK_InvalidParameter;
    }

	m_fileID			= in_fileID;

    AKASSERT( m_pszStreamName == NULL );
    
    const AkUInt32 uLLBlockSize = in_pDevice->GetLowLevelHook()->GetBlockSize( *in_pFileDesc );
    if ( !uLLBlockSize
		|| uLLBlockSize > in_uGranularity
		|| ( in_uGranularity % uLLBlockSize ) > 0 )
    {
		AK_MONITOR_ERROR( AK::Monitor::ErrorCode_IncompatibleIOSettings );
		AKASSERT( !"Invalid Low-Level I/O block size. Must be >= 1 && <= Granularity and a multiple of the granularity" );
		SetToBeDestroyed();
        return AK_Fail;
    }
	m_uLLBlockSize = uLLBlockSize;
	m_uBufferAlignment = uLLBlockSize;

	// Heuristics.
	SetThroughput(AkMax( in_heuristics.fThroughput, AK_MINIMAL_THROUGHPUT ));
    m_uLoopStart	= in_heuristics.uLoopStart - ( in_heuristics.uLoopStart % m_uLLBlockSize );;
	if ( in_heuristics.uLoopEnd <= in_pFileDesc->iFileSize )
		m_uLoopEnd      = in_heuristics.uLoopEnd;
	else
		m_uLoopEnd		= (AkUInt32)in_pFileDesc->iFileSize;
    
	// Minimum number of buffers concurrently owned by client is 1 when not specified.
	m_uMinNumBuffers = ( in_heuristics.uMinNumBuffers > 0 ) ? in_heuristics.uMinNumBuffers : 1;

    m_priority      = in_heuristics.priority;
    
    m_uNextExpectedUserPosition	= 0;

	if ( in_pFileDesc->iFileSize == 0 )
		SetReachedEof( true );

    AKRESULT eResult = SetBufferingSettings( in_pBufferSettings, in_uGranularity );
	if ( eResult != AK_Success )
		SetToBeDestroyed();

	// IMPORTANT: Do not set m_pFileDesc here.
	AKASSERT( !m_pFileDesc );

	return eResult;
}

static inline AkUInt32 Gcd( AkUInt32 a, AkUInt32 b )
{
	AKASSERT( a > 0 && b > 0 );
	if ( a < b ) 
	{
		AkUInt32 tmp = a;
		a = b;
		b = tmp;
	}
	
	AkUInt32 r = a % b;
	a = b;
	b = r;
	if ( b==0 ) return a;

	AkUInt32 k = 0;
	while ( !((a|b)&1) ) // both even
	{
		k++;
		a >>= 1;
		b >>= 1;
	}
	while ( !(a&1) ) a >>= 1;
	while ( !(b&1) ) b >>= 1;
	do
	{
		if ( a==b ) return a << k;
		if ( a < b )
		{
			AkUInt32 tmp = a;
			a = b;
			b = tmp;
		}
		AkUInt32 t = (a-b) >> 1; // t>0
		while ( !(t&1) ) t >>= 1;
		a = t;
	}
	while ( 1 );
}

// Helper: Set stream's buffering constraints.
// Sync: Status.
AKRESULT CAkAutoStmBase::SetBufferingSettings(
	const AkAutoStmBufSettings * in_pBufferSettings,
	AkUInt32 in_uGranularity
	)
{
	// Set up buffer size according to streaming memory constraints.
	if ( in_pBufferSettings )
	{
		// Fix buffer alignment if specified. It is the smallest common multiple between user and low-level block sizes.
		if ( in_pBufferSettings->uBlockSize > 0 )
			m_uBufferAlignment = m_uLLBlockSize * ( in_pBufferSettings->uBlockSize / Gcd( m_uLLBlockSize, in_pBufferSettings->uBlockSize ) );

		// Default values: Align buffer with block size, 
		// min buffer size is equal to block size.
		m_uBufferSize = in_uGranularity - ( in_uGranularity % m_uBufferAlignment );
		m_uMinBufferSize = m_uBufferAlignment;	
		
		// Buffer size constraints.
        if ( in_pBufferSettings->uBufferSize != 0 )
        {
			// User constrained buffer size. Ensure that it is valid with device granularity and low-level block size.
            if ( in_pBufferSettings->uBufferSize > in_uGranularity 
				|| ( in_pBufferSettings->uBufferSize % m_uBufferAlignment ) > 0 )
            {
				AK_MONITOR_ERROR( AK::Monitor::ErrorCode_IncompatibleIOSettings );
                return AK_Fail;
            }
            m_uBufferSize = in_pBufferSettings->uBufferSize;
			m_uMinBufferSize = in_pBufferSettings->uBufferSize;	// Hard constraint: min buffer size is equal to specified buffer size.
        }
        else
        {
			// Minimum buffer size specified?
			if ( in_pBufferSettings->uMinBufferSize > 0 )
			{
				// Min buffer size is equal to specified min buffer size if specified, low-level block size otherwise.
				if ( m_uMinBufferSize < in_pBufferSettings->uMinBufferSize )
				{
					m_uMinBufferSize = in_pBufferSettings->uMinBufferSize;	
					// Snap to required alignment, and then check against buffer size.
					m_uMinBufferSize = ( ( m_uMinBufferSize + m_uBufferAlignment - 1 ) / m_uBufferAlignment ) * m_uBufferAlignment;
					if ( m_uMinBufferSize > m_uBufferSize )
					{
						AK_MONITOR_ERROR( AK::Monitor::ErrorCode_IncompatibleIOSettings );
						return AK_Fail;
					}
				}
			}
        }
    }
	else
	{
		// Default values: Align buffer with block size, 
		// min buffer size is equal to block size.
		m_uBufferSize = in_uGranularity - ( in_uGranularity % m_uLLBlockSize );
		m_uMinBufferSize = m_uLLBlockSize;
	}

	return AK_Success;
}

//-----------------------------------------------------------------------------
// IAkAutoStream interface.
//-----------------------------------------------------------------------------

// Destruction.
// Close stream. The object is destroyed and the interface becomes invalid.
// Buffers are flushed here.
// Sync: 
// 1. Locks scheduler to set its m_bIsToBeDestroued flag. Better lock the
// scheduler once in a while than having the scheduler lock all the streams
// every time it schedules a task. Also, it spends most of its time executing I/O,
// so risks of interlock are minimized.
// 2. Lock status.
void CAkAutoStmBase::Destroy()
{
	AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

	SetToBeDestroyed();

	CHECK_BUFFERING_CONSISTENCY();

	// Client can have buffers still granted to him. Clear them before flushing.
	// More efficient just to clear m_uNextToGrant while adjusting the virtual buffering value, then flush all.
	AkBufferList::Iterator it = m_listBuffers.Begin();
	while ( m_uNextToGrant > 0 )
	{
		AKASSERT( it != m_listBuffers.End() );
		m_uVirtualBufferingSize += GetEffectiveViewSize( (*it) );
		--m_uNextToGrant;
		++it;
	}

	Flush();	// Scheduling status is updated in Flush().
    
	m_listBuffers.Term();
}

// Stream info access.
// Sync: None.
void CAkAutoStmBase::GetInfo(
    AkStreamInfo & out_info       // Returned stream info.
    )
{
    AKASSERT( m_pDevice != NULL );
    out_info.deviceID	= m_pDevice->GetDeviceID( );
    out_info.pszName	= m_pszStreamName;
    out_info.uSize		= m_pFileDesc->iFileSize;
	out_info.bIsOpen	= m_bIsFileOpen;
}

// Stream heuristics access.
// Sync: None.
void CAkAutoStmBase::GetHeuristics(
    AkAutoStmHeuristics & out_heuristics    // Returned stream heuristics.
    )
{
    out_heuristics.fThroughput  = GetThroughput();
    out_heuristics.uLoopStart   = m_uLoopStart;
    out_heuristics.uLoopEnd     = m_uLoopEnd;
    out_heuristics.uMinNumBuffers   = m_uMinNumBuffers;
    out_heuristics.priority     = m_priority;
}

// Stream heuristics run-time change.
// Sync: None.
AKRESULT CAkAutoStmBase::SetHeuristics(
    const AkAutoStmHeuristics & in_heuristics   // New stream heuristics.
    )
{
	if (SetPriority(in_heuristics.priority) != AK_Success)
    {
        return AK_InvalidParameter;
    }

	//
	// Update heuristics that have an effect on scheduling.
    //
	AkReal32 fNewThroughput = AkMax( in_heuristics.fThroughput, AK_MINIMAL_THROUGHPUT );

	// Looping.
	//
	AkUInt32 uLoopEnd;
	if ( in_heuristics.uLoopEnd <= m_pFileDesc->iFileSize 
		|| !m_bIsFileOpen )
		uLoopEnd = in_heuristics.uLoopEnd;
	else
		uLoopEnd = (AkUInt32)m_pFileDesc->iFileSize;
    if ( m_uLoopEnd != uLoopEnd ||
         m_uLoopStart != in_heuristics.uLoopStart )
    {
        // Lock status.
        AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );
        
        
        // Update other heuristics.
		SetThroughput(fNewThroughput);
		// Note: Minimum number of buffers concurrently owned by client is 1 when not specified.
		m_uMinNumBuffers = ( in_heuristics.uMinNumBuffers > 0 ) ? in_heuristics.uMinNumBuffers : 1;

		// Snap loop start to block size and store new heuristics.
		AkUInt32 uLoopStart = in_heuristics.uLoopStart - ( in_heuristics.uLoopStart % m_uLLBlockSize );
	    m_uLoopStart    = uLoopStart;
	    SetLoopEnd( uLoopEnd );	// Loop end changed: recompute virtual buffering from scratch.

		// Inspect all the current memory views, ready and pending. Flush anything that is incompatible
		// with the new looping heuristics.

		// 1) Get first buffer not granted.
		AkUInt64 uNextExpectedPosition = m_uNextExpectedUserPosition;
		AkBufferList::IteratorEx it = m_listBuffers.BeginEx();
		AkUInt32 uLastBufferIdxToKeep = 0;
		AKASSERT( m_listBuffers.Length() >= m_uNextToGrant );
		while ( uLastBufferIdxToKeep < m_uNextToGrant ) // skip buffers that are already granted
        {
			uNextExpectedPosition = (*it)->EndPosition();
			++it;
			++uLastBufferIdxToKeep;
		}
		if ( uLoopEnd > 0 && uNextExpectedPosition >= uLoopEnd )
			uNextExpectedPosition = uLoopStart;

		// 2) Iterate through list of buffers already streamed in, and dequeue any of them that is not 
		// consistent with the next expected position. Store them temporarily in a separate queue in order
		// to release them all at once.
		AkStmMemViewListLight listToRemove;
		while ( it != m_listBuffers.End() )
		{
			CAkStmMemView * pMemView = (*it);

			if ( pMemView->StartPosition() != uNextExpectedPosition )
			{
				// Dequeue and add to listToRemove.				
				it = m_listBuffers.Erase( it );
				listToRemove.AddFirst( pMemView );
			}
			else
			{
				// Valid. Keep it and update uNextExpectedPosition.
				uNextExpectedPosition = pMemView->EndPosition();
				if ( uLoopEnd > 0 && uNextExpectedPosition >= uLoopEnd )
					uNextExpectedPosition = uLoopStart;
				++it;
			}
		}

		// 3) Release all dequed buffers (must be inside device lock: memory change).
		if ( !listToRemove.IsEmpty() )
		{
			AkAutoLock<CAkIOThread> lock( *m_pDevice );

			AkStmMemViewListLight::IteratorEx itRemove = listToRemove.BeginEx();
			while ( itRemove != listToRemove.End() )
			{
				CAkStmMemView * pMemView = (*itRemove);
				itRemove = listToRemove.Erase( itRemove );
				DestroyBuffer( pMemView );
			}
		}

		listToRemove.Term();

		// 4) Do the same with pending transfers.
		CancelInconsistentPendingTransfers( uNextExpectedPosition );
		
		UpdateSchedulingStatus();
    }
	else	// looping changed
	{
		// Update other heuristics that have an effect on scheduling only if their value changed.

		// Note: Minimum number of buffers concurrently owned by client is 1 when not specified.
		AkUInt8 uNewMinNumBuffers = ( in_heuristics.uMinNumBuffers > 0 ) ? in_heuristics.uMinNumBuffers : 1;

		if ( GetThroughput() != fNewThroughput
			|| m_uMinNumBuffers != uNewMinNumBuffers )
		{
			AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

			m_uMinNumBuffers	= uNewMinNumBuffers;
			SetThroughput(fNewThroughput);

			UpdateSchedulingStatus();
		}		
	}
    return AK_Success;
}

AKRESULT CAkAutoStmBase::SetPriority(
	const AkPriority in_priority   ///< New stream priority
	)
{
	if ( in_priority < AK_MIN_PRIORITY ||
		in_priority > AK_MAX_PRIORITY )
	{
		AKASSERT( !"Invalid stream heuristics" );
		return AK_InvalidParameter;
	}

	m_priority = in_priority;

	return AK_Success;
}

// Run-time change of the stream's minimum buffer size that can be handed out to client.
// Sync: Status.
AKRESULT CAkAutoStmBase::SetMinimalBufferSize(
	AkUInt32 in_uMinBufferSize	// Minimum buffer size that can be handed out to client.
	)
{
	AkUInt32 uOldMinBufferSize = m_uMinBufferSize;
	
	AkAutoStmBufSettings bufferSettings;
	bufferSettings.uBufferSize	= 0; // Unconstrained.
	bufferSettings.uMinBufferSize = in_uMinBufferSize;
	bufferSettings.uBlockSize	= 0; // Unconstrained.

	// Lock status.
	AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

	AKRESULT eResult = SetBufferingSettings( &bufferSettings, m_pDevice->GetGranularity() );

	if ( eResult == AK_Success )
	{
		if ( m_uMinBufferSize > uOldMinBufferSize )
		{
			// Now that buffering constraints have been changed, pass through buffer list and flush everything
			// after an invalid buffer size.
			FlushSmallBuffersAndPendingTransfers( in_uMinBufferSize );
			UpdateSchedulingStatus();
		}
	}
	else
		UpdateTaskStatus( AK_Fail );

	return eResult;
}

// Name the stream (appears in Wwise profiler).
// Sync: None.
AKRESULT CAkAutoStmBase::SetStreamName(
    const AkOSChar * in_pszStreamName    // Stream name.
    )
{
    if ( m_pszStreamName != NULL )
        AkFree( CAkStreamMgr::GetObjPoolID(), m_pszStreamName );

    if ( in_pszStreamName != NULL )
    {
        // Allocate string buffer for user defined stream name.
		size_t uStrLen = AKPLATFORM::OsStrLen( in_pszStreamName ) + 1;
        m_pszStreamName = (AkOSChar*)AkAlloc( CAkStreamMgr::GetObjPoolID(), uStrLen * sizeof(AkOSChar) );
        if ( m_pszStreamName == NULL )
            return AK_InsufficientMemory;

        // Copy.
		AKPLATFORM::SafeStrCpy( m_pszStreamName, in_pszStreamName, uStrLen );
    }
    return AK_Success;
}

// Returns low-level IO block size for this stream's file descriptor.
AkUInt32 CAkAutoStmBase::GetBlockSize()
{
    return m_uLLBlockSize;
}

// Operations.
// ---------------------------------------

// Starts automatic scheduling.
// Sync: Status update if not already running. 
// Notifies memory change.
AKRESULT CAkAutoStmBase::Start()
{
    if ( !m_bIsRunning )
    {
		{
			// UpdateSchedulingStatus() will notify scheduler if required.
			AkAutoLock<CAkLock> status( m_lockStatus );
			SetRunning( true );
			UpdateSchedulingStatus();
			m_bCachingReady = true;
			
			// Reset time. Time count since last transfer starts now.
			m_iIOStartTime = m_pDevice->GetTime();
		}

        // The scheduler should reevaluate memory usage. Notify it.
		{
			AkAutoLock<CAkIOThread> lock( *m_pDevice );
	        m_pDevice->NotifyMemChange();
		}
	}

    return m_bIOError ? AK_Fail : AK_Success;
}

// Stops automatic scheduling.
// Sync: Status update.
AKRESULT CAkAutoStmBase::Stop()
{
	// Lock status.
    AkAutoLock<CAkLock> status( m_lockStatus );

    SetRunning( false );
	Flush();
    
    return AK_Success;
}

// Get stream position; position as seen by the user.
AkUInt64 CAkAutoStmBase::GetPosition( 
    bool * out_pbEndOfStream    // Can be NULL.
    )
{
	AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

	AkUInt64 uCurPosition;
	if ( !m_listBuffers.IsEmpty() )
		uCurPosition = m_listBuffers.First()->StartPosition();
	else
		uCurPosition = m_uNextExpectedUserPosition;

    if ( out_pbEndOfStream != NULL )
        *out_pbEndOfStream = ( uCurPosition >= (AkUInt64)m_pFileDesc->iFileSize );

    return uCurPosition;
}

// Set stream position. Modifies position of next read/write.
// Sync: Updates status. 
AKRESULT CAkAutoStmBase::SetPosition(
    AkInt64         in_iMoveOffset,     // Seek offset.
    AkMoveMethod    in_eMoveMethod,     // Seek method, from beginning, end or current file position.
    AkInt64 *       out_piRealOffset    // Actual seek offset may differ from expected value when unbuffered IO.
                                        // In that case, floors to sector boundary. Pass NULL if don't care.
    )
{
    if ( out_piRealOffset != NULL )
    {
        *out_piRealOffset = 0;
    }

    // Compute position.
    AkInt64 iPosition;
    if ( in_eMoveMethod == AK_MoveBegin )
    {
        iPosition = in_iMoveOffset;
    }
    else if ( in_eMoveMethod == AK_MoveCurrent )
    {
		iPosition = GetPosition( NULL ) + in_iMoveOffset;
    }
    else if ( in_eMoveMethod == AK_MoveEnd )
    {
        iPosition = m_pFileDesc->iFileSize + in_iMoveOffset;
    }
    else
    {
        AKASSERT( !"Invalid move method" );
        return AK_InvalidParameter;
    }

    if ( iPosition < 0 )
    {
        AKASSERT( !"Trying to move the file pointer before the beginning of the file" );
        return AK_InvalidParameter;
    }

    // Change offset if Low-Level block size is greater than 1.
    if ( iPosition % m_uLLBlockSize != 0 )
    {
        // Round down to block size.
        iPosition -= ( iPosition % m_uLLBlockSize );
        AKASSERT( iPosition >= 0 );
    }

    // Set real offset if argument specified.
    if ( out_piRealOffset != NULL )
    {
        switch ( in_eMoveMethod )
        {
        case AK_MoveBegin:
            *out_piRealOffset = iPosition;
            break;
        case AK_MoveCurrent:
            *out_piRealOffset = iPosition - GetPosition( NULL );
            break;
        case AK_MoveEnd:
            *out_piRealOffset = iPosition - m_pFileDesc->iFileSize;
            break;
        default:
            AKASSERT( !"Invalid move method" );
        }
    }

    // Set new position and update status. 
    ForceFilePosition( iPosition );

    return AK_Success;
}

// Cache query:
// Returns true if data for the next expected transfer was found either in the list or in cache.
// Returns false if data is found neither in the list or cache.
// If data is found in the list or cache, out_pBuffer is set and ready to use. 
// Sync: Stream needs to be locked.
bool CAkAutoStmBase::GetBufferOrReserveCacheBlock( void *& out_pBuffer, AkUInt32 & out_uSize )
{
	if ( !m_bIsFileOpen )
		return false;

	// Check in buffer list first.
	out_pBuffer = GetReadBuffer( out_uSize );

	if ( out_pBuffer )
		return true;


	// Not in buffer; Try getting from cache.
	if ( m_pDevice->ExecuteCachedTransfer( this ) )
		out_pBuffer = GetReadBuffer( out_uSize );

	return ( out_pBuffer != NULL );
}

// Data/status access. 
// -----------------------------------------

// GetBuffer.
// Return values : 
// AK_DataReady     : if buffer is granted.
// AK_NoDataReady   : if buffer is not granted yet.
// AK_NoMoreData    : if buffer is granted but reached end of file (next will return with size 0).
// AK_Fail          : there was an IO error. 

// Sync: Updates status.
AKRESULT CAkAutoStmBase::GetBuffer(
    void *&         out_pBuffer,        // Address of granted data space.
    AkUInt32 &      out_uSize,          // Size of granted data space.
    bool            in_bWait            // Block until data is ready.
    )
{
    out_pBuffer    = NULL;
    out_uSize       = 0;

    // Get data from list of buffers already streamed in.
	{
		m_lockStatus.Lock();
	    out_pBuffer = GetReadBuffer( out_uSize );
	
		// Data ready?
		if ( !out_pBuffer
			&& !m_bIOError )
		{
			AKASSERT( ( m_bIsRunning || !in_bWait ) || !"Trying to block on GetBuffer() on a stopped stream" );

			

			// Handle blocking GetBuffer. No data is ready, but there is more data to come
			// (otherwise out_pBuffer would not be NULL).
			
			// Retry getting buffer. 
			if ( GetBufferOrReserveCacheBlock( out_pBuffer, out_uSize ) 
				|| in_bWait )	// IMPORTANT: in_bWait needs to be 2nd because GetBufferOrReserveCacheBlock() HAS TO BE called.
			{
				while ( !out_pBuffer 
						&& !m_bIOError
						&& !NeedsNoMoreTransfer( 0 ) )	// Pass 0 "Actual buffering size": We know we don't have any.
				{
					// Wait for I/O to complete if there is no error.
					// Set as "blocked, waiting for I/O".
					SetBlockedStatus();

					// Release lock and let the scheduler perform IO.
					m_lockStatus.Unlock();

					m_pDevice->WaitForIOCompletion( this );
					
					// Get status lock. Try get buffer again. If it returns nothing, then wait for I/O to complete.
					m_lockStatus.Lock();
					out_pBuffer = GetReadBuffer( out_uSize );
				}
			}
		}
		m_lockStatus.Unlock();
	}
	    
    AKRESULT eRetCode;
    if ( m_bIOError )
    {
        eRetCode = AK_Fail;
    }
    else if ( out_pBuffer == NULL )
    {
        // Buffer is empty, either because no data is ready, or because scheduling has completed 
        // and there is no more data.
		if ( m_bHasReachedEof 
			&& m_uNextExpectedUserPosition >= FileSize() )
            eRetCode = AK_NoMoreData;
        else
		{
			AKASSERT( !in_bWait || !"Blocking GetBuffer() cannot return AK_NoDataReady" );
            eRetCode = AK_NoDataReady;
		}
    }
    else
    {
		// Return AK_NoMoreData if buffering reached EOF and this is going to be the last buffer granted.
		if ( m_bHasReachedEof 
			&& m_uNextExpectedUserPosition >= FileSize() )
            eRetCode = AK_NoMoreData;
        else
            eRetCode = AK_DataReady;
    }
    return eRetCode;
}

// Release buffer granted to user. Returns AK_Fail if there are no buffer granted to client.
// Sync: Status lock.
AKRESULT CAkAutoStmBase::ReleaseBuffer()
{
    // Lock status.
    AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

    // Release first buffer granted to client. 
	if ( m_uNextToGrant > 0 )
    {
		CAkStmMemView * pFirst = m_listBuffers.First();
		AKASSERT( pFirst );

		// Note: I/O pool access must be enclosed in scheduler lock.
		{
			AkAutoLock<CAkIOThread> lock( *m_pDevice );
			
			AKVERIFY( m_listBuffers.RemoveFirst() == AK_Success );
			m_pDevice->DestroyMemView( pFirst );
		}
        
        // Update "next to grant" index.
        m_uNextToGrant--;

		UpdateSchedulingStatus();

		return AK_Success;
    }

	// Failure: Buffer was not found or not granted.
	return AK_Fail;
}

//Return true if more data needs to be requested
AkUInt32 CAkAutoStmBase::GetVirtualBufferingSize()
{
	return m_uVirtualBufferingSize;
}

void CAkAutoStmBase::StartCaching()
{
	if ( m_bCachingReady )
		Start();
}

AkUInt32 CAkAutoStmBase::StopCaching(AkUInt32 in_uMemNeededForTask)
{
	// Lock status.
	AkAutoLock<CAkLock> status( m_lockStatus );

	AkUInt32 uMemFreed = ReleaseCachingBuffers(in_uMemNeededForTask);

	SetRunning( false );
	UpdateSchedulingStatus();

#ifndef AK_OPTIMIZED
	//Prevent stopped caching streams from appearing as active in the profiler.
	m_bCanClearActiveProfile = true;
#endif

	return uMemFreed;
}

void CAkAutoStmBase::SetCachingBufferSize(AkUInt32 in_uNumBytes)
{
	AKASSERT(m_bIsCachingStream);
	m_uCachingBufferSize = AkMax( AK_CACHING_STREAM_MIN_BUFFER_SIZE, (((in_uNumBytes - 1) / m_uBufferAlignment) + 1 ) * m_uBufferAlignment );
}

AkUInt32 CAkAutoStmBase::ReleaseCachingBuffers(AkUInt32 in_uTargetMemToRecover)
{
	AkUInt32 uMemFreed = 0;
	
	if ( uMemFreed < in_uTargetMemToRecover && m_listBuffers.Length() > 0 )
	{
		AkAutoLock<CAkIOThread> lock( *m_pDevice ); 		// Lock scheduler for memory change.

		CAkStmMemView * pLast = m_listBuffers.Last();
		while (pLast != NULL && uMemFreed < in_uTargetMemToRecover) 
		{
			uMemFreed += pLast->Size();
			AKVERIFY( m_listBuffers.Remove(pLast) );
			DestroyBuffer( pLast );

			pLast = m_listBuffers.Last();
		}
	}

	return uMemFreed;
}

// Get the amount of buffering that the stream has. 
// Returns
// - AK_DataReady: Some data has been buffered (out_uNumBytesAvailable is greater than 0).
// - AK_NoDataReady: No data is available, and the end of file has not been reached.
// - AK_NoMoreData: Some or no data is available, but the end of file has been reached. The stream will not buffer any more data.
// - AK_Fail: The stream is invalid due to an I/O error.
AKRESULT CAkAutoStmBase::QueryBufferingStatus( AkUInt32 & out_uNumBytesAvailable )
{
	if ( AK_EXPECT_FALSE( m_bIOError ) )
		return AK_Fail;

	// Lock status to query buffering.
	AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

	if ( AK_EXPECT_FALSE( !m_bIsFileOpen ) )
		return AK_NoDataReady;

	AKRESULT eRetCode;
	bool bBufferingReady;
	do 
	{
		eRetCode = CalcUnconsumedBufferSize( out_uNumBytesAvailable );

		// Try to see if it is readily available in cache as long is it needs more data. 
		bBufferingReady = NeedsNoMoreTransfer( out_uNumBytesAvailable );

	} while ( 
		!bBufferingReady
		&& m_pDevice->ExecuteCachedTransfer( this ) );

	
	// Deal with end of stream: return AK_NoMoreData if we are not going to stream in any more data,
	// or if the device currently cannot stream in anymore data. Clients must be aware that the device
	// is idle to avoid hangs.
	if ( bBufferingReady
		|| m_pDevice->CannotScheduleAutoStreams() )
	{
		eRetCode = AK_NoMoreData;
	}

	return eRetCode;
}

AKRESULT CAkAutoStmBase::CalcUnconsumedBufferSize(AkUInt32 & out_uNumBytesAvailable)
{
	AKRESULT eRetCode = AK_NoDataReady;

	out_uNumBytesAvailable = 0;

	// Skip granted buffers.
	AkBufferList::Iterator it = m_listBuffers.Begin();
	AkUInt32 uIdx = 0;
	AKASSERT( m_uNextToGrant <= m_listBuffers.Length() );
	AkUInt32 uSkip = m_uNextToGrant;
	while ( uIdx < uSkip )
	{
		++uIdx;
		++it;
	}

	// Count in buffers that are already there.
	while ( it != m_listBuffers.End() )
	{
		out_uNumBytesAvailable += (*it)->Size();
		eRetCode = AK_DataReady;
		++it;
	}

	return eRetCode;
}

// Returns the target buffering size based on the throughput heuristic.
AkUInt32 CAkAutoStmBase::GetNominalBuffering()
{
	AkUInt32 uNominalBuffering;
	
	if (m_bIsCachingStream)
	{
		uNominalBuffering= m_uCachingBufferSize;
	}
	else
	{
		uNominalBuffering= (AkUInt32)(m_pDevice->GetTargetAutoStmBufferLength() * m_fThroughput);
	}

	return uNominalBuffering;
}



//-----------------------------------------------------------------------------
// CAkStmTask implementation.
//-----------------------------------------------------------------------------

// This is called when file size is set after deferred open. Stream object implementations may
// perform any update required after file size was set. 
// Automatic stream object ensure that the loop end heuristic is consistent.
void CAkAutoStmBase::OnFileDeferredOpen()
{
	CAkStmTask::OnFileDeferredOpen();

	AkAutoStmHeuristics heuristics;
	GetHeuristics( heuristics );
	if ( heuristics.uLoopEnd > m_pFileDesc->iFileSize )
	{
		heuristics.uLoopEnd = (AkUInt32)m_pFileDesc->iFileSize;
		AKVERIFY( SetHeuristics( heuristics ) == AK_Success );
	}
}

// Add a new streaming memory view (or content of a memory view) to this stream after a transfer.
// If it ends up not being used, it is disposed of. Otherwise it's status is set to Ready.
// All logical transfers must end up here, even if they were cancelled.
// Sync: Status must be locked prior to calling this function. 
void CAkAutoStmBase::AddMemView( 
	CAkStmMemView * in_pMemView,		// Transfer-mode memory view to resolve.
	bool			in_bStoreData		// Store data in stream object only if true.
	)
{
	AKASSERT( in_pMemView );

	if ( in_bStoreData 
		&& !m_bIsToBeDestroyed 
		&& !m_bIOError )
    {
		AkUInt32 uTransferSize = in_pMemView->Size();
		AKASSERT( uTransferSize > 0 );
		// Data size cannot be passed the end of file. 
		AKASSERT ( in_pMemView->EndPosition() <= FileSize() );

		// Profiling. 
#ifndef AK_OPTIMIZED
		{
			m_uBytesTransfered += uTransferSize;

			bool bIsFromLowLevel = in_pMemView->Status() != CAkStmMemView::TransferStatus_Ready;
			if (bIsFromLowLevel)
				m_uBytesTransferedLowLevel += uTransferSize;	// Was not already ready: required low-level transfer.

			// Push transfer statistics only if monitoring, as this requires unwanted locking.
			if (m_pDevice->IsMonitoring())
				m_pDevice->PushTransferStatistics(uTransferSize, bIsFromLowLevel);
		}
#endif

		// Add buffer to list.
		in_pMemView->TagAsReady();
		m_listBuffers.AddLast( in_pMemView );
    }
    else
    {
		// Stream was either scheduled to be destroyed while I/O was occurring, stopped, or 
        // its position was set dirty while I/O was occuring. Flush that data.

		// Note: I/O pool access must be enclosed in scheduler lock.
		AkAutoLock<CAkIOThread> lock( *m_pDevice );

		DestroyBuffer( in_pMemView );
    }
}

// Update task's status after transfer.
void CAkAutoStmBase::UpdateTaskStatus(
	AKRESULT	in_eIOResult			// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
	)
{
    // Status.
    if ( AK_Fail == in_eIOResult )
    {
        // Set to Error.
        m_bIOError = true;

        // Stop auto stream.
        Stop();
    }

	// Update scheduling status.
    UpdateSchedulingStatus();

    // Release the client thread if an operation was pending and call was blocking on it.
    if ( IsBlocked() )
    {
		m_pDevice->SignalIOCompleted( this );
    }

}

void CAkAutoStmBase::Kill()
{
	AkAutoLock<CAkLock> updateStatus( m_lockStatus );
	UpdateTaskStatus( AK_Fail );
}

// Compute task's deadline for next operation.
// Sync: None, but copy throughput heuristic on the stack.
AkReal32 CAkAutoStmBase::EffectiveDeadline()
{
	// Note: Sync. These values might be changed by another thread. 
	// In the worst case, the scheduler will take a sub-optimal decision.
	AkReal32 fThroughput= GetThroughput();
	AKASSERT( fThroughput >= AK_MINIMAL_THROUGHPUT );
	return m_uVirtualBufferingSize / fThroughput;
}

//-----------------------------------------------------------------------------
// Profiling.
//-----------------------------------------------------------------------------
#ifndef AK_OPTIMIZED
// Profiling: get data.
void CAkAutoStmBase::GetStreamData(
    AkStreamData & out_streamData
    )
{
	AkAutoLock<CAkLock> stmBufferGate(m_lockStatus);

    out_streamData.uStreamID = m_uStreamID;
    out_streamData.uPriority = m_priority;
	out_streamData.uFilePosition = m_uNextExpectedUserPosition;
	out_streamData.uVirtualBufferingSize = m_uVirtualBufferingSize;
	// IMPORTANT: Target buffering size logic should be kept in sync with NeedsBuffering.
	out_streamData.uTargetBufferingSize = GetNominalBuffering();
	
	if (m_bIsCachingStream)
	{
		out_streamData.fEstimatedThroughput= 0.f;
	}
	else
	{
		out_streamData.fEstimatedThroughput = m_fThroughput;
	}

	out_streamData.uNumBytesTransfered = m_uBytesTransfered;
	out_streamData.uNumBytesTransferedLowLevel = m_uBytesTransferedLowLevel;
	out_streamData.bActive = m_bWasActive;

	out_streamData.uMemoryReferenced = 0;
	out_streamData.uBufferedSize = 0;
	{
		AkUInt32 uNextToGrant = m_uNextToGrant;

		// Add up memory
		for (AkBufferList::Iterator it = m_listBuffers.Begin(); it != m_listBuffers.End(); ++it)
		{
			out_streamData.uMemoryReferenced += (*it)->AllocSize();

			if (uNextToGrant == 0)
				out_streamData.uBufferedSize += GetEffectiveViewSize(*it);
			else
				uNextToGrant--;
		}

		if ( m_bCanClearActiveProfile )
			m_bWasActive = false;
	}

	// Clamp buffering size to target.
	if (out_streamData.uVirtualBufferingSize > out_streamData.uTargetBufferingSize)
		out_streamData.uVirtualBufferingSize = out_streamData.uTargetBufferingSize;	// Clamp amount of available data to target buffer length.

	if (out_streamData.uBufferedSize > out_streamData.uTargetBufferingSize)
		out_streamData.uBufferedSize = out_streamData.uTargetBufferingSize;	// Clamp amount of available data to target buffer length.

	m_uBytesTransfered = 0;    // Reset.
	m_uBytesTransferedLowLevel = 0;
}

// Signals that stream can be destroyed.
void CAkAutoStmBase::ProfileAllowDestruction()	
{
    AKASSERT( m_bIsToBeDestroyed );
    m_bIsProfileDestructionAllowed = true;
	AkAutoLock<CAkLock> statusChange( m_lockStatus );
	UpdateSchedulingStatus();
}
#endif

//-----------------------------------------------------------------------------
// Helpers.
//-----------------------------------------------------------------------------
// Update task status.
// Sync: Status lock.
void CAkAutoStmBase::ForceFilePosition(
    const AkUInt64 in_uNewPosition     // New stream position.
    )
{
    // Lock status.
    AkAutoLock<CAkLock> statusGate( m_lockStatus );

	// Update position.
	m_uNextExpectedUserPosition = in_uNewPosition;

    // Check whether next buffer position corresponds to desired position (if SetPosition() was consistent
	// with looping heuristic, it will be). If it isn't, then we need to flush everything we have.

	if ( m_uNextToGrant < m_listBuffers.Length() )
	{
		if ( GetNextBufferToGrant()->StartPosition() != in_uNewPosition )
    	{
			// Flush everything we have, that was not already granted to user.
			// Note: Flush() also flushes pending transfers.
			Flush();
			AKASSERT( m_listBuffers.Length() == m_uNextToGrant );
    	}
		else
			UpdateSchedulingStatus();
    }
	else 
	{
		// Nothing buffered. Yet, there might be pending transfers that are inconsistent with in_uNewPosition.
		// Cancel all pending transfers if applicable.
		CancelInconsistentPendingTransfers( in_uNewPosition );
		UpdateSchedulingStatus();
	}
}

// Update task scheduling status; whether or not it is waiting for I/O and counts in scheduler semaphore.
// Sync: Status MUST be locked from outside.
void CAkAutoStmBase::UpdateSchedulingStatus()
{
	CHECK_BUFFERING_CONSISTENCY();

	// Set EOF flag.
	if ( !m_uLoopEnd 
		&& ( GetVirtualFilePosition() >= FileSize() )
		&& m_bIsFileOpen )
    {
        SetReachedEof( true );
    }
    else
		SetReachedEof( false );

    // Update scheduler control.
	if ( ( ReadyForIO() && NeedsBuffering( m_uVirtualBufferingSize ) )	// requires a transfer.
		|| ( IsToBeDestroyed() && CanBeDestroyed() ) )	// requires clean up.
    {
        if ( !m_bRequiresScheduling )
        {
			m_bRequiresScheduling = true;
#ifndef AK_OPTIMIZED
			m_bWasActive = true;
			m_bCanClearActiveProfile = false;
#endif
            m_pDevice->AutoSemIncr();
        }
    }
    else
    {
        if ( m_bRequiresScheduling )
        {
			m_bRequiresScheduling = false;
            m_pDevice->AutoSemDecr();
        }
    }
}

// Compares in_uVirtualBufferingSize to target buffer size.
// Important: Keep computation of target buffer size in sync with GetStreamData().
bool CAkAutoStmBase::NeedsBuffering(
	AkUInt32 in_uVirtualBufferingSize
	)
{
	// Needs buffering if below target buffer length.
	return ( in_uVirtualBufferingSize < GetNominalBuffering() );
}

// Returns a buffer filled with data. NULL if no data is ready.
// Sync: Accessing list. Must be locked from outside.
void * CAkAutoStmBase::GetReadBuffer(     
    AkUInt32 & out_uSize                // Buffer size.
    )
{
	AKASSERT( m_uNextToGrant < 255 || !"Cannot get more than 255 buffers at once" );

    if ( m_uNextToGrant < m_listBuffers.Length() )
    {
		// Get first buffer not granted.
		CAkStmMemView * pMemView = GetNextBufferToGrant();

		if ( pMemView->StartPosition() != m_uNextExpectedUserPosition )
        {
            // User attempts to read a buffer passed the end loop point heuristic, but did not set the
            // stream position accordingly!
			// This should never occur if the client is consistent with the heuristics it sets.
            // Flush data reset looping heuristics for user to avoid repeating this mistake, return AK_NoDataReady.
            SetLoopEnd( 0 );
			Flush();
            out_uSize = 0;
            return NULL;
        }
        
        // Update "next to grant" index.
		m_uNextToGrant++;

		// Update m_iNextExpectedUserPosition.
		m_uNextExpectedUserPosition = pMemView->EndPosition();

		// Update amount of buffered data (data granted to user does not count as buffered data).
		out_uSize = pMemView->Size();
		m_uVirtualBufferingSize -= GetEffectiveViewSize( pMemView );

		UpdateSchedulingStatus();
        
        return pMemView->Buffer();
    }
    
    // No data ready.
    out_uSize = 0;
    return NULL;
}

// Flushes all stream buffers that are not currently granted.
// Sync: None. Always called from within status-protected code.
void CAkAutoStmBase::Flush()
{
	CHECK_BUFFERING_CONSISTENCY();

	CancelAllPendingTransfers();

    if ( m_listBuffers.Length() > m_uNextToGrant )
    {
		AkUInt32 uIdx = 0;
		AkBufferList::IteratorEx it = m_listBuffers.BeginEx();
		while ( uIdx < m_uNextToGrant )
	    {
			++uIdx;
			++it;
		}
        
		// Lock scheduler for memory change.
		AkAutoLock<CAkIOThread> lock( *m_pDevice );
        while ( it != m_listBuffers.End() )
        {
			CAkStmMemView * pMemView = (*it);
			it = m_listBuffers.Erase( it );
			DestroyBuffer( pMemView );
        }
    }

	UpdateSchedulingStatus();
}
