//////////////////////////////////////////////////////////////////////
//
// AkIOThread.h
//
// Platform-specific layer of high-level I/O devices of the Stream Manager.
// Implements the I/O thread function, manages all thread related 
// synchronization.
// Specific to POSIX API.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_IO_THREAD_H_
#define _AK_IO_THREAD_H_

#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <pthread.h>

// ------------------------------------------------------------------------------
// Defines.
// ------------------------------------------------------------------------------

namespace AK
{
namespace StreamMgr
{
	//-----------------------------------------------------------------------------
    // Name: class CAkClientThreadAware
    // Desc: Platform-specific layer for stream tasks which handles blocking client
	//		 threads for blocking I/O.
	//		 Client threads are blocked using a conditional variable
	//		 and signaling it for a specific thread (sys_cond_signal_to).
    //-----------------------------------------------------------------------------
	class CAkClientThreadAware : public CAkObject
	{
    public:

		// Platform-independent interface.
		// Platform-independent implementation calls these methods.
		//-----------------------------------------------------------------------------
		CAkClientThreadAware();
		virtual ~CAkClientThreadAware();
		
		inline void ClearBlockedStatus() 
		{ 
			m_idBlockedThread = AK_NULL_THREAD; 
		}
		
		void SetBlockedStatus();
		
		inline bool IsBlocked() 
		{ 
			return m_idBlockedThread != AK_NULL_THREAD; 
		}
		
		// Platform-dependent interface.
		// CAkIOThread uses these methods.
		//-----------------------------------------------------------------------------
		inline pthread_t BlockedThreadID() { return m_idBlockedThread; }

	private:
        pthread_t	m_idBlockedThread;	// ID of thread that is waiting for I/O to complete.	
    };


    //-----------------------------------------------------------------------------
    // Name: CAkIOThread
    // Desc: Implements the I/O thread, and synchronization services. Namely,
	//		 thread control based on standard and automatic streams semaphores,
	//		 memory full, max number of concurrent low-level transfers. It also 
	//		 works with client thread blocking (partly implemented in 
	//		 CAkClientThreadAware).
    //       The I/O thread calls pure virtual method PerformIO(), that has to be
    //       implemented by derived classes according to how they communicate with
    //       the Low-Level IO.
    //-----------------------------------------------------------------------------
    class CAkIOThread : public CAkObject
    {
    public:

        CAkIOThread();
        virtual ~CAkIOThread();

		// Methods used by stream objects.
        // --------------------------------------------------------

		// Blocking I/O.
		void WaitForIOCompletion( 
			CAkClientThreadAware * in_pWaitingTask // Stream objects must call this method with this for synchronization purposes:
			);								// the thread id must be checked after the mutex was obtained to avoid race conditions.
		void SignalIOCompleted(
			CAkClientThreadAware * in_pWaitingTask	// The caller's thread id must be checked after the mutex was obtained to avoid race conditions.
			);								// CAkStmTask::ClearBlockedStatus() will be called from within the critical section.

        // Scheduler thread control.
        /* Semaphore usage:
        StdSem: Signaled whenever at least one standard stream is waiting for IO (pending operation).
        AutoSem: Signaled whenever at least one automatic stream is waiting for IO (running), and memory
        usage allows it.
        Memory notifications: When set to "idle", AutoSem event is inhibated: the I/O thread will not 
        wake up to schedule automatic streams. This occurs when there is no more memory, and the scheduler
        decides not to reassign buffers.
        It is signaled whenever a streaming buffer is freed (ReleaseBuffer(), Destroy(), SetPosition()->Flush()),
        or if a new stream is created (requires memory usage reevaluation), through NotifyMemChange().
        */
        void StdSemIncr();      // Increment standard streams semaphore count (signal).
        void StdSemDecr();      // Decrement standard streams semaphore count (reset if count=0).
        void AutoSemIncr();     // Increment automatic streams semaphore count (signal).
        void AutoSemDecr();     // Decrement automatic streams semaphore count (reset if count=0).
        // Warning. Memory notifications (below) are not protected. They must be enclosed in a 
		// CAkIOThread::Lock()/Unlock() pair along with I/O pool allocs and frees. 
        void NotifyMemChange(); // Notify that a streaming buffer was freed.
        void NotifyMemIdle();   // Notify that all streaming buffers are used and should not be reassigned
                                // (inhibates AutoSem).

		// Query memory status.
		inline bool CannotScheduleAutoStreams()
		{
			return m_bDoWaitMemoryChange;
		}

		// Scheduler control locking: semaphores (stream counts, pending I/O count, ...) must be updated 
		// atomically. Use this lock when it is not possible to do so.
        // Also, need to enclose I/O memory allocation and notification inside this lock.
        // Note: It is legal to call these from streams' status locked sections, but the opposite is illegal:
        // Do not try to obtain a stream's status lock with Mem locked.
		inline void Lock()
		{
			AKVERIFY( pthread_mutex_lock( &m_mutexPendingStmsSem ) == 0 );
		}
		inline void Unlock()
		{
			AKVERIFY( pthread_mutex_unlock( &m_mutexPendingStmsSem ) == 0 );
		}

		// Concurrent I/O requests.
		// TODO Move this to a policy class template: MultiTransferPolicy.
		void IncrementIOCount();
		void DecrementIOCount();
		inline void UnsafeIncrementIOCount() { ++m_uNumConcurrentIO; }
		inline void UnsafeDecrementIOCount() { AKASSERT( m_uNumConcurrentIO > 0 ); --m_uNumConcurrentIO; }
		inline AkUInt32 GetNumConcurrentIO() { return m_uNumConcurrentIO; }
        
	protected:
		// Init/term scheduler objects.
        AKRESULT Init( 
            const AkThreadProperties & in_threadProperties 
            );
        void     Term();

		// I/O Scheduler.
        static AK_DECLARE_THREAD_ROUTINE( IOSchedThread );

        // I/O Algorithm. Depends on the type of device. 
        // Implement in derived class.
        virtual void PerformIO() = 0;

		// Destroys all streams.
        virtual bool ClearStreams() = 0;

		// Called once when I/O thread starts.
		virtual void OnThreadStart() = 0;

		// The I/O thread asks the device if it is ready to schedule through this method.
		// If it is not, wait for the CondVar.
		inline bool CanSchedule()
		{
			return ( m_uNumConcurrentIO < m_uMaxConcurrentIO &&
					( m_cPendingStdStms > 0 
					|| ( m_cRunningAutoStms > 0 
						&& !m_bDoWaitMemoryChange ) ) );
		}

	private:
		void ClearThreadData();

	protected:
		// Thread level settings.
		AkUInt32        m_uMaxConcurrentIO;

		// Attributes for synchronization.
	private:

		// IO thread handle.
        AkThread			m_hIOThread;
		// IO thread semaphore.
		pthread_mutex_t		m_mutexPendingStmsSem;	// Mutex: used for scheduler control critical section.
		pthread_cond_t		m_condAreTasksPending;	// Conditional variable. If the condition (read inside the critical section) for 
													// performing an I/O transfer is false, the I/O thread waits on this critical
													// variable until this variable is signaled (when a change occurs).
        AkUInt32			m_cPendingStdStms;		// Number of standard stream tasks waiting for I/O. 
        AkUInt32			m_cRunningAutoStms;		// Number of automatic stream tasks waiting for I/O.
        bool				m_bDoWaitMemoryChange;	// When true, automatic streams semaphore is inhibated, because there is no free buffers, and they cannot be reassigned. 
#ifdef _DEBUG
        bool				m_bIsAutoSemSignaled;   // True when semaphore is signaled (result of m_cRunningAutoStms>0 and "don't sleep on memory"). For debug purposes.
#endif
        
        // TODO Move this to a policy class template: MultiTransferPolicy.
		AkUInt32			m_uNumConcurrentIO;		// Number of concurrent requests sent to LowLevelIO.

		// Synchronization objects for blocking I/O.
		pthread_mutex_t		m_mutexBlockingIO;
		pthread_cond_t		m_condBlockingIODone;
		AkThreadProperties	m_threadProperties;

        // Stop condition.
        bool			m_bDoRun;
    };
}
}
#endif //_AK_DEVICE_BASE_H_
