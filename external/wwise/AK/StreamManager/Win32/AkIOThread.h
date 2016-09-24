//////////////////////////////////////////////////////////////////////
//
// AkIOThread.h
//
// Platform-specific layer of high-level I/O devices of the Stream Manager.
// Implements the I/O thread function, manages all thread related 
// synchronization.
// Specific to Win32 API.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_IO_THREAD_H_
#define _AK_IO_THREAD_H_

#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkAutoLock.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

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
	//		 On Win32 based platforms, client threads are blocked using a 
	//		 manual event.
    //-----------------------------------------------------------------------------
	class CAkClientThreadAware
	{
    public:

		// Platform-independent interface.
		// Platform-independent implementation calls these methods.
		//-----------------------------------------------------------------------------
		CAkClientThreadAware();
		virtual ~CAkClientThreadAware();
		
		inline void ClearBlockedStatus() 
		{ 
			m_bIsBlocked = false; 
		}
		
		void SetBlockedStatus();
		
		inline bool IsBlocked() 
		{ 
			return m_bIsBlocked; 
		}
		
		// Platform-dependent interface.
		// CAkIOThread uses these methods.
		//-----------------------------------------------------------------------------
		inline HANDLE BlockEvent() { return m_hBlockEvent; }

	private:
        HANDLE              m_hBlockEvent;      // Event used for blocking I/O.
        bool				m_bIsBlocked;		// Blocked status: True when thread will be waiting for I/O completion.
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
    class CAkIOThread
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
                                // (inhibits AutoSem).

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
			m_lockSems.Lock();
		}
		inline void Unlock()
		{
			m_lockSems.Unlock();
		}

		// Concurrent I/O requests.
		// TODO Move this to a policy class template: MultiTransferPolicy.
		void IncrementIOCount();
		void DecrementIOCount();
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

		inline bool BelowMaxConcurrentIO()
		{
			AkAutoLock<CAkIOThread> safeCheck( *this );
			return ( m_uNumConcurrentIO < m_uMaxConcurrentIO );
		}

	protected:

		// Thread level settings.
		AkUInt32        m_uMaxConcurrentIO;

		// Attributes for synchronization.
	private:
		// IO thread handle.
        HANDLE          m_hIOThread;
        
        // IO thread semaphore.
		// Locks for semaphores (event+count atomic).
        // Memory notifications must be atomic with allocs and frees, and with the AutoSem, because it uses
        // the same event.
        // Note. Alloc/Free/Mem notifications mechanism works well because alloc sizes are always the same: freeing
        // ensures that next allocation will succeed.
        CAkLock         m_lockSems;				// Lock for stream count and IO request count.
        HANDLE          m_hIOThreadStopEvent;	// Event; signaled when the I/O thread must stop.
		HANDLE	 		m_hMaxIOGate;			// Event; signaled when the current transfer count is below uMaxConcurrentIO.
        HANDLE          m_hStdSem;              // Event; signaled when at least one standard stream task is ready.
        HANDLE          m_hAutoSem;             // Event; signaled when at least one automatic stream task is ready.
        AkInt32         m_cPendingStdStms;      // Number of standard stream tasks waiting for I/O. When it reaches 0, m_hStdSem is reset.
        AkInt32         m_cRunningAutoStms;     // Number of automatic stream tasks waiting for I/O. When it reaches 0, m_hAutoSem is reset.
        bool            m_bDoWaitMemoryChange;  // When true, automatic streams semaphore is inhibated, because there is no free buffers, and they cannot be reassigned. 
#ifdef _DEBUG
        bool            m_bIsAutoSemSignaled;   // True when semaphore is signaled (result of m_cRunningAutoStms>0 and "don't sleep on memory"). For debug purposes.
#endif
        
		// TODO Move this to a policy class template: MultiTransferPolicy.
		AkUInt32		m_uNumConcurrentIO;		// Number of concurrent requests sent to LowLevelIO.
    };

}
}
#endif //_AK_DEVICE_BASE_H_
