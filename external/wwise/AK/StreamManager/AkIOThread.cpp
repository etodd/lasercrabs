#if _WIN32
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

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>
#include "AkIOThread.h"
#include <AK/Tools/Common/AkAutoLock.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

using namespace AK::StreamMgr;

//-----------------------------------------------------------------------------
// Name: class CAkClientThreadAware
// Desc: Platform-specific layer for stream tasks which handles blocking client
//		 threads for blocking I/O.
//		 On Win32 based platforms, client threads are blocked using a 
//		 manual event.
//-----------------------------------------------------------------------------

CAkClientThreadAware::CAkClientThreadAware()
: m_hBlockEvent( NULL )
, m_bIsBlocked( false )
{
}

CAkClientThreadAware::~CAkClientThreadAware()
{
	if ( m_hBlockEvent )
	{
		::CloseHandle( m_hBlockEvent );
		m_hBlockEvent = NULL;
	}
}

// Set task as "blocked, waiting for I/O completion".
void CAkClientThreadAware::SetBlockedStatus() 
{ 
	// Reset event. Create it if it was not created yet.
	if ( m_hBlockEvent )
		AKVERIFY( ::ResetEvent( m_hBlockEvent ) );
	else
	{
#ifdef AK_USE_METRO_API
		m_hBlockEvent = ::CreateEventEx( nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, STANDARD_RIGHTS_ALL|EVENT_MODIFY_STATE );
#else
        // Note. Event is manual reset in case IO thread had time to execute IO before we block on it.
        m_hBlockEvent = ::CreateEvent( NULL,    // no security
                                       TRUE,    // manual reset
                                       FALSE,   // unsignaled
                                       NULL );  // no name
#endif
        AKASSERT( m_hBlockEvent || !"Failed creating event for blocking IO" );
	}
	m_bIsBlocked = true; 
}


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

// Device construction/destruction.
CAkIOThread::CAkIOThread()
: m_bDoWaitMemoryChange( false )
#ifdef _DEBUG
, m_bIsAutoSemSignaled( false )
#endif
, m_hIOThreadStopEvent( NULL )
, m_hIOThread( NULL )
, m_hStdSem( NULL )
, m_hAutoSem( NULL )
, m_hMaxIOGate( NULL )
, m_cPendingStdStms( 0 )
, m_cRunningAutoStms( 0 )
, m_uNumConcurrentIO( 0 )
{
}

CAkIOThread::~CAkIOThread()
{
}

// Init/term scheduler objects.
AKRESULT CAkIOThread::Init( 
    const AkThreadProperties & in_threadProperties // Platform-specific thread properties. Can be NULL (uses default).
    )
{
    // Create scheduler semaphore.
#ifdef AK_USE_METRO_API
		m_hStdSem = ::CreateEventEx( nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, STANDARD_RIGHTS_ALL|EVENT_MODIFY_STATE );
		m_hAutoSem = ::CreateEventEx( nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, STANDARD_RIGHTS_ALL|EVENT_MODIFY_STATE );
		m_hIOThreadStopEvent = ::CreateEventEx( nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, STANDARD_RIGHTS_ALL|EVENT_MODIFY_STATE );
		m_hMaxIOGate = ::CreateEventEx( nullptr, nullptr, CREATE_EVENT_MANUAL_RESET | CREATE_EVENT_INITIAL_SET, STANDARD_RIGHTS_ALL|EVENT_MODIFY_STATE );
#else
    m_hStdSem = ::CreateEvent( NULL,
                               TRUE,    // Manual reset.
                               FALSE,   // Initially not signaled.
                               NULL );
    
    m_hAutoSem = ::CreateEvent( NULL,
                                TRUE,    // Manual reset.
                                FALSE,   // Initially not signaled.
                                NULL );
    
    // Create stop event.
    m_hIOThreadStopEvent = ::CreateEvent( NULL,
                                          TRUE,		// Manual reset
                                          FALSE,	// Initially non signaled
                                          NULL		// No name
                                          );

	// IO Count event.
	m_hMaxIOGate = ::CreateEvent( NULL,
                                  TRUE,  // Manual reset
                                  TRUE,  // Initially signaled
                                  NULL    // No name
                                  );
#endif
	m_cPendingStdStms = 0;
    m_cRunningAutoStms = 0;
	m_uNumConcurrentIO = 0;

    // Launch the scheduler/IO thread.
    // Create and start the worker IO thread with default stack size.
	AKPLATFORM::AkCreateThread( 
		CAkIOThread::IOSchedThread, 
		this, 
		in_threadProperties,
		&m_hIOThread,
		"AK::IOThread" );

    if ( !AKPLATFORM::AkIsValidThread(&m_hIOThread) ||
		 !m_hIOThreadStopEvent || 
         !m_hStdSem || 
         !m_hAutoSem )
    {
        return AK_Fail;
    }

    return AK_Success;
}

void CAkIOThread::Term()
{
    // If it exists, signal stop event.
    if ( m_hIOThreadStopEvent )
    {
        AKVERIFY( ::SetEvent( m_hIOThreadStopEvent ) );

        // Wait until thread stops.
        if ( m_hIOThread )
        {
#ifdef AK_USE_METRO_API
			DWORD dwWait = ::WaitForSingleObjectEx( m_hIOThread, INFINITE, FALSE );  // 3 secs.
#else
			DWORD dwWait = ::WaitForSingleObject( m_hIOThread, INFINITE );  // 3 secs.
#endif
			AKASSERT( WAIT_OBJECT_0 == dwWait || !"I/O thread did not properly terminate" );

            // Close events and thread handles.
            AKVERIFY( ::CloseHandle( m_hIOThread ) );
            m_hIOThread = NULL;
        }
        AKASSERT( m_hIOThreadStopEvent );
        AKVERIFY( ::CloseHandle( m_hIOThreadStopEvent ) );
        m_hIOThreadStopEvent = NULL;
    }
    // Destroy thread handle if it exists.
    if ( m_hIOThread )
    {
        ::CloseHandle( m_hIOThread );
        m_hIOThread = NULL;
    }
    // Destroy scheduler semaphore.
    if ( m_hStdSem )
    {
        ::CloseHandle( m_hStdSem );
        m_hStdSem = NULL;
    }
    m_cPendingStdStms = 0;
    if ( m_hAutoSem )
    {
        ::CloseHandle( m_hAutoSem );
        m_hAutoSem = NULL;
    }
    m_cRunningAutoStms = 0;
    
	if ( m_hMaxIOGate )
    {
		::CloseHandle( m_hMaxIOGate );
		m_hMaxIOGate = NULL;
	}
}

// Scheduler thread.
// --------------------------------------------------------

AK_DECLARE_THREAD_ROUTINE( CAkIOThread::IOSchedThread )
{
    CAkIOThread * pDevice = AK_GET_THREAD_ROUTINE_PARAMETER_PTR( CAkIOThread );

    bool bRun = true;
    DWORD dwWaitRes;

    HANDLE pEvents[3];
    pEvents[0] = pDevice->m_hIOThreadStopEvent;
    pEvents[1] = pDevice->m_hStdSem; // Signaled when at least one standard stream task is ready.
    pEvents[2] = pDevice->m_hAutoSem;

	HANDLE pEventStopOrBelowMaxIO[2];
	pEventStopOrBelowMaxIO[0] = pDevice->m_hIOThreadStopEvent;
	pEventStopOrBelowMaxIO[1] = pDevice->m_hMaxIOGate;

	pDevice->OnThreadStart();

    while ( bRun )
    {
        // We could be awaken to perform I/O for one reason or another. But we must block on 
		// the "max IO" event, in an alertable state, in order to allow the transfers to complete.
		// If we are below uMaxConcurrentIO, "max IO" event is signaled and we'll pass through.
		dwWaitRes = ::WaitForMultipleObjectsEx(
			2,
			pEventStopOrBelowMaxIO,
			FALSE,    // !bWaitAll
			INFINITE, 
			TRUE );	// alertable
		// WAIT_OBJECT_0 is stop, WAIT_OBJECT_0+1 is "release max IO gate". Will perform IO below.

		if ( dwWaitRes != WAIT_OBJECT_0 )	// !stop
		{
			dwWaitRes = ::WaitForMultipleObjectsEx( 
				3,
				pEvents,
				FALSE,    // !bWaitAll
				AK_INFINITE,
				TRUE );
		}

        switch ( dwWaitRes )
        {
        case WAIT_OBJECT_0:
            // Stop
			if ( pDevice->ClearStreams() )
	            bRun = false;
			else
			{
				// Sleep in alertable state, to let a chance for pending transfers to complete.
#ifdef AK_USE_THREAD_EMULATION
				AK::ThreadEmulation::SleepEx( 100, TRUE );
#else
				::SleepEx( 100, TRUE );
#endif
			}
            break;
        case WAIT_TIMEOUT:      // Idle throughput
        case WAIT_OBJECT_0+1:   // Standard streams semaphore
        case WAIT_OBJECT_0+2:   // Automatic streams semaphore
            // Schedule.
			// Note: We could have been awaken from second wait function. Check m_uNumConcurrentIO again
			// and pass through first wait function if we are over the limit.
			if ( pDevice->BelowMaxConcurrentIO() )
	            pDevice->PerformIO();
            break;
		case WAIT_IO_COMPLETION:
			// Skip
			break;
        default:
            AKASSERT( !"Fatal error in I/O device thread" );
            return AK_RETURN_THREAD_ERROR;

        }
    }
    return AK_RETURN_THREAD_OK;
}

//
// Methods used by stream objects: 
// Scheduler thread control.
// -----------------------------------

// Increment pending standard streams count.
// Sync: Standard streams semaphore interlock.
void CAkIOThread::StdSemIncr()
{
    AkAutoLock<CAkLock> gate( m_lockSems );
    if ( ++m_cPendingStdStms == 1 )
      ::SetEvent( m_hStdSem );

}

// Decrement pending standard streams count.
// Sync: Standard streams semaphore interlock.
void CAkIOThread::StdSemDecr()
{
    AkAutoLock<CAkLock> gate( m_lockSems );
    AKASSERT( m_cPendingStdStms > 0 );
    if ( --m_cPendingStdStms == 0 )
        ::ResetEvent( m_hStdSem );
}

// Increment pending automatic streams count.
// Note: Event remains unsignaled if there is not memory ("Mem Idle" state).
// Sync: Automatic streams semaphore lock.
void CAkIOThread::AutoSemIncr()
{
    AkAutoLock<CAkLock> gate( m_lockSems );
    m_cRunningAutoStms++;
    if ( m_cRunningAutoStms == 1 &&
         !m_bDoWaitMemoryChange )
    {
        // We just incremented it from 0 to 1 and memory is available. Signal event.
#ifdef _DEBUG
        m_bIsAutoSemSignaled = true;
#endif
        ::SetEvent( m_hAutoSem );
    }
}

// Decrement pending automatic streams count.
// Sync: Automatic streams semaphore lock.
void CAkIOThread::AutoSemDecr()
{
    AkAutoLock<CAkLock> gate( m_lockSems );
    AKASSERT( m_cRunningAutoStms > 0 );
    m_cRunningAutoStms--;    
    if ( m_cRunningAutoStms == 0 )
    {
        // We just decremented it from 1 to 0. Reset event.
#ifdef _DEBUG
        m_bIsAutoSemSignaled = false;
#endif
        ::ResetEvent( m_hAutoSem );
    }
}

// Notify that memory was freed, or memory usage must be reviewed.
// Un-inhibates automatic streams semaphore. Event is signaled if pending automatic streams count is greater than 0.
// IMPORTANT Sync: None. Locking must be handled on the caller side, to enclose calls to Memory Manager
// and protect automatic streams semaphore. Tasks use LockMem().
void CAkIOThread::NotifyMemChange()
{
    if ( m_bDoWaitMemoryChange )
    {
        m_bDoWaitMemoryChange = false;
        if ( m_cRunningAutoStms > 0 )
        {
            // Auto streams are running and we just notified that some memory is available. Signal event.
#ifdef _DEBUG
            m_bIsAutoSemSignaled = true;
#endif
            ::SetEvent( m_hAutoSem );
        }
    }
}

// Notify that memory is idle. I/O thread should not wake up to service automatic streams until memory usage
// changes (until someone calls NotifyMemChange).
// Inhibates automatic streams semaphore.
// IMPORTANT Sync: None. Locking must be handled on the caller side, to enclose calls to Memory Manager
// and protect automatic streams semaphore. Tasks use LockMem().
void CAkIOThread::NotifyMemIdle()
{
    m_bDoWaitMemoryChange = true;
    if ( m_cRunningAutoStms > 0 )
    {
        // Auto streams are running but we just notified that memory is not available. Reset event.
#ifdef _DEBUG
        m_bIsAutoSemSignaled = false;
#endif
        ::ResetEvent( m_hAutoSem );
    }
}

// Concurrent I/O requests.
// --------------------------------------------------------
void CAkIOThread::IncrementIOCount()
{
	AkAutoLock<CAkLock> gate(m_lockSems);
	++m_uNumConcurrentIO;
	if ( m_uNumConcurrentIO >= m_uMaxConcurrentIO )
		::ResetEvent( m_hMaxIOGate );
}

void CAkIOThread::DecrementIOCount()
{
	AkAutoLock<CAkLock> gate(m_lockSems);
	AKASSERT( m_uNumConcurrentIO > 0 );
	--m_uNumConcurrentIO;
	if ( m_uNumConcurrentIO == m_uMaxConcurrentIO - 1 )
		::SetEvent( m_hMaxIOGate );
}

// Blocking I/O.
// --------------------------------------------------------
void CAkIOThread::WaitForIOCompletion( 
	CAkClientThreadAware * in_pWaitingTask
	)
{
	AKASSERT( in_pWaitingTask->BlockEvent() );
	// Note: in_pWaitingTask->IsBlocked() can be false if SignalIOCompleted() was called before we arrived here.
	// It is OK, since the event remains signaled for the call to WaitForSingleObject() just below, which is going to pass through.
#ifdef AK_USE_METRO_API
	DWORD dwWaitResult = ::WaitForSingleObjectEx( in_pWaitingTask->BlockEvent(), INFINITE, FALSE );
	AKASSERT( dwWaitResult == WAIT_OBJECT_0 );
#else
	AKVERIFY( ::WaitForSingleObject( in_pWaitingTask->BlockEvent(), INFINITE ) == WAIT_OBJECT_0 );
#endif
}

void CAkIOThread::SignalIOCompleted(
	CAkClientThreadAware * in_pWaitingTask
	)
{
	AKASSERT( in_pWaitingTask->BlockEvent() && in_pWaitingTask->IsBlocked() );
	in_pWaitingTask->ClearBlockedStatus();
	::SetEvent( in_pWaitingTask->BlockEvent() );
}


#else
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

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>
#include "AkIOThread.h"
#include <AK/Tools/Common/AkAutoLock.h>
#include <errno.h>
#include <sys/time.h>


using namespace AK::StreamMgr;

//-----------------------------------------------------------------------------
    // Name: class CAkClientThreadAware
    // Desc: Platform-specific layer for stream tasks which handles blocking client
	//		 threads for blocking I/O.
	//		 Client threads are blocked using a conditional variable
	//		 and signaling it for a specific thread (sys_cond_signal_to).
    //-----------------------------------------------------------------------------

CAkClientThreadAware::CAkClientThreadAware()
: m_idBlockedThread( AK_NULL_THREAD )
{
}

CAkClientThreadAware::~CAkClientThreadAware()
{
	AKASSERT( !IsBlocked() );
}

// Set task as "blocked, waiting for I/O completion".
void CAkClientThreadAware::SetBlockedStatus() 
{ 
	// Get current thread's id. Having a non-zero m_idBlockedThread indicates that this stream is waiting for a signal.
	AKASSERT( m_idBlockedThread == AK_NULL_THREAD );
	m_idBlockedThread = pthread_self();	
}


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

// Device construction/destruction.
CAkIOThread::CAkIOThread()
: m_bDoWaitMemoryChange( false )
#ifdef _DEBUG
, m_bIsAutoSemSignaled( false )
#endif
, m_cPendingStdStms( 0 )
, m_cRunningAutoStms( 0 )
, m_uNumConcurrentIO( 0 )
, m_hIOThread( AK_NULL_THREAD )

{	
	AKPLATFORM::AkClearThread( &m_hIOThread );
	ClearThreadData();
}

CAkIOThread::~CAkIOThread()
{
}

void CAkIOThread::ClearThreadData()
{
	memset( &m_mutexPendingStmsSem, 0, sizeof( pthread_mutex_t) );
	memset( &m_condAreTasksPending,	0, sizeof( pthread_cond_t) );
	memset( &m_mutexBlockingIO,		0, sizeof( pthread_mutex_t) );
	memset( &m_condBlockingIODone,	0, sizeof( pthread_cond_t) );
}

// Init/term scheduler objects.
AKRESULT CAkIOThread::Init( 
    const AkThreadProperties & in_threadProperties // Platform-specific thread properties. Can be NULL (uses default).
    )
{
	// Initialize client thread blocking synchronization objects.
	{
		pthread_mutexattr_t mutex_attr;
		AKVERIFY(! pthread_mutexattr_init( &mutex_attr ));
		
#ifndef AK_NACL
		pthread_condattr_t cond_attr;
		AKVERIFY(! pthread_condattr_init( &cond_attr ));
#endif

		if ( pthread_mutex_init( &m_mutexBlockingIO, &mutex_attr ) != 0 )
			return AK_Fail;
#ifndef AK_NACL
		if ( pthread_cond_init( &m_condBlockingIODone, &cond_attr ) != 0 )
			return AK_Fail;
#else
		if ( pthread_cond_init( &m_condBlockingIODone, NULL ) != 0 )
			return AK_Fail;
#endif
		
		AKVERIFY(! pthread_mutexattr_destroy(&mutex_attr));
#ifndef AK_NACL
		AKVERIFY(! pthread_condattr_destroy(&cond_attr));
#endif
	}

	// Create scheduler semaphore.
	{
		pthread_mutexattr_t mutex_attr;

		AKVERIFY(!pthread_mutexattr_init( &mutex_attr ));
		
		AKVERIFY(!pthread_mutexattr_settype( &mutex_attr, PTHREAD_MUTEX_RECURSIVE ));
#ifndef AK_NACL
		pthread_condattr_t cond_attr;
		AKVERIFY(!pthread_condattr_init( &cond_attr ));
#endif
		if ( pthread_mutex_init( &m_mutexPendingStmsSem, &mutex_attr ) != 0 )
			return AK_Fail;
		
#ifndef AK_NACL
		if ( pthread_cond_init( &m_condAreTasksPending, &cond_attr ) != 0 )
			return AK_Fail;
#else
		if ( pthread_cond_init( &m_condAreTasksPending, NULL ) != 0 )
			return AK_Fail;
#endif
		
		AKVERIFY(! pthread_mutexattr_destroy(&mutex_attr));
#ifndef AK_NACL
		AKVERIFY(! pthread_condattr_destroy(&cond_attr));
#endif
	}

	m_cPendingStdStms	= 0;
    m_cRunningAutoStms	= 0;
	m_uNumConcurrentIO	= 0;
    
    // Launch the scheduler/IO thread.
    // Create and start the worker IO thread with default stack size.
	
	m_threadProperties = in_threadProperties;
	m_bDoRun = true;
	AKPLATFORM::AkCreateThread(
		CAkIOThread::IOSchedThread,			// Start address
		this,								// Parameter
		in_threadProperties,				// Properties
		&m_hIOThread,						// Handle
		"AK::IOThread" );	

	if ( AKPLATFORM::AkIsValidThread( &m_hIOThread ) )
		return AK_Success;		
	return AK_Fail;
}

void CAkIOThread::Term()
{
	// It is ok that the functions return EINVAL 
	// because we might pass twice in the Term function
	int ret = pthread_mutex_lock( &m_mutexPendingStmsSem );
	AKASSERT( ret == 0 || ret == EINVAL );
	m_bDoRun = false;
	ret = pthread_cond_signal(&m_condAreTasksPending);
	AKASSERT( ret == 0 || ret == EINVAL );
	ret = pthread_mutex_unlock( &m_mutexPendingStmsSem );
	AKASSERT( ret == 0 || ret == EINVAL );
	
	
	if ( AKPLATFORM::AkIsValidThread( &m_hIOThread ) )
    {
		AKPLATFORM::AkWaitForSingleThread( &m_hIOThread );
		m_hIOThread = AK_NULL_THREAD;
	}
	
	ret = pthread_mutex_destroy( &m_mutexPendingStmsSem );
	AKASSERT( ret == 0 || ret == EINVAL );
	ret = pthread_cond_destroy( &m_condAreTasksPending );
	AKASSERT( ret == 0 || ret == EINVAL );
	ret = pthread_mutex_destroy( &m_mutexBlockingIO );
	AKASSERT( ret == 0 || ret == EINVAL );
	ret = pthread_cond_destroy( &m_condBlockingIODone );
	AKASSERT( ret == 0 || ret == EINVAL );
	
	ClearThreadData();
}

// Scheduler thread.
// --------------------------------------------------------
AK_DECLARE_THREAD_ROUTINE( CAkIOThread::IOSchedThread )
{	
    CAkIOThread * pDevice = AK_GET_THREAD_ROUTINE_PARAMETER_PTR( CAkIOThread );
	AK_THREAD_INIT_CODE(pDevice->m_threadProperties);

	struct timespec   ts;
	struct timeval    tp;
	
	pDevice->OnThreadStart();
    
    while ( true )
    {
		// Enter critical section.
		// The I/O thread checks if tasks are waiting to be serviced. If there are none, wait for CondVar signal.
		pDevice->Lock();
		
		while ( !pDevice->CanSchedule()
				&& pDevice->m_bDoRun )
		{
			int ret = pthread_cond_wait( &pDevice->m_condAreTasksPending,  &pDevice->m_mutexPendingStmsSem );
			AKASSERT( ret == 0 || !"Error while waiting for condition variable" );
		}

		bool bStop = !( pDevice->m_bDoRun );
		
		pDevice->Unlock();
		
		if ( bStop )
		{
			if ( pDevice->ClearStreams() )
			{
				AkExitThread(AK_RETURN_THREAD_OK);
			}
			else
				AKPLATFORM::AkSleep( 100 );
		}

		// Schedule.
        pDevice->PerformIO();
    }
}

//
// Methods used by stream objects: 
// Scheduler thread control.
// -----------------------------------

// Increment pending standard streams count.
// Sync: Standard streams semaphore interlock.
void CAkIOThread::StdSemIncr()
{
    AkAutoLock<CAkIOThread> chgSem( *this );
	
	if ( ++m_cPendingStdStms == 1 )
	{
		// We just incremented it from 0 to 1. Signal condition variable.
		AKVERIFY( pthread_cond_signal( &m_condAreTasksPending ) == 0 );
	}
}

// Decrement pending standard streams count.
// Sync: Standard streams semaphore interlock.
void CAkIOThread::StdSemDecr()
{
    AkAutoLock<CAkIOThread> chgSem( *this );
	AKASSERT( m_cPendingStdStms > 0 );
    --m_cPendingStdStms;
}

// Increment pending automatic streams count.
// Note: Event remains unsignaled if there is no memory ("Mem Idle" state).
// Sync: Streams semaphore lock (mutex). Signals CondVar if I/O thread was idle.
void CAkIOThread::AutoSemIncr()
{
	AkAutoLock<CAkIOThread> chgSem( *this );

	m_cRunningAutoStms++;
    if ( m_cRunningAutoStms == 1 &&
         !m_bDoWaitMemoryChange )
    {
        // We just incremented it from 0 to 1 and memory is available. 
		// Signal CondVar to release the I/O if necessary.
#ifdef _DEBUG
        m_bIsAutoSemSignaled = true;
#endif
		AKVERIFY( pthread_cond_signal( &m_condAreTasksPending ) == 0 );
    }
}

// Decrement pending automatic streams count.
// Sync: Automatic streams semaphore lock.
void CAkIOThread::AutoSemDecr()
{
	AkAutoLock<CAkIOThread> chgSem( *this );

    AKASSERT( m_cRunningAutoStms > 0 );
    m_cRunningAutoStms--;    
#ifdef _DEBUG
	// We just decremented it from 1 to 0. I/O thread will stop performing I/O on automatic streams.
    m_bIsAutoSemSignaled = ( m_cRunningAutoStms > 0 );
#endif
}

// Notify that memory was freed, or memory usage must be reviewed.
// Un-inhibates automatic streams semaphore. Event is signaled if pending automatic streams count is greater than 0.
// IMPORTANT Sync: None. Locking must be handled on the caller side, to enclose calls to Memory Manager
// and protect automatic streams semaphore. Tasks use LockMem().
void CAkIOThread::NotifyMemChange()
{
    if ( m_bDoWaitMemoryChange )
    {
        m_bDoWaitMemoryChange = false;
        if ( m_cRunningAutoStms > 0 )
        {
            // Auto streams are running and we just notified that some memory is available. 
			// Signal CondVar to release the I/O if necessary.
#ifdef _DEBUG
            m_bIsAutoSemSignaled = true;
#endif
			AKVERIFY( pthread_cond_signal( &m_condAreTasksPending ) == 0 );
        }
    }
}

// Notify that memory is idle. I/O thread should not wake up to service automatic streams until memory usage
// changes (until someone calls NotifyMemChange).
// Inhibates automatic streams semaphore.
// IMPORTANT Sync: None. Locking must be handled on the caller side, to enclose calls to Memory Manager
// and protect automatic streams semaphore. Tasks use LockMem().
void CAkIOThread::NotifyMemIdle()
{
    m_bDoWaitMemoryChange = true;
#ifdef _DEBUG
	if ( m_cRunningAutoStms > 0 )
    {
        // Auto streams are running but we just notified that memory is not available. I/O thread will stop
		// performing I/O on automatic streams.
        m_bIsAutoSemSignaled = false;
    }
#endif
}

// Concurrent I/O requests.
// --------------------------------------------------------
void CAkIOThread::IncrementIOCount()
{
	AkAutoLock<CAkIOThread> chgSem( *this );
	++m_uNumConcurrentIO;
}

void CAkIOThread::DecrementIOCount()
{
	AkAutoLock<CAkIOThread> chgSem( *this );
	--m_uNumConcurrentIO;
	AKVERIFY( pthread_cond_signal( &m_condAreTasksPending ) == 0 );
}

// Blocking I/O.
// --------------------------------------------------------
void CAkIOThread::WaitForIOCompletion( 
	CAkClientThreadAware * in_pWaitingTask
	)
{
	AKVERIFY( pthread_mutex_lock( &m_mutexBlockingIO ) == 0 );

	// Now that we aquired the mutex, check if the task is still waiting for I/O (its thread ID would be NULL).
	// Since all blocked threads are awaken when a blocking I/O completes, they need to check their status again
	// (BlockedThreadID() not being null) before leaving.
	while ( in_pWaitingTask->BlockedThreadID() != AK_NULL_THREAD )
	{
		// Still waiting for I/O: wait on conditional variable signal.
		AKVERIFY( pthread_cond_wait( &m_condBlockingIODone, &m_mutexBlockingIO ) == 0 );		
	}

	AKVERIFY( pthread_mutex_unlock( &m_mutexBlockingIO ) == 0 );
}

void CAkIOThread::SignalIOCompleted(
	CAkClientThreadAware * in_pWaitingTask	// The caller's thread id must be checked after the mutex was obtained to avoid race conditions.
	)								// CAkStmTask::ClearBlockedStatus() will be called from within the critical section.
{
	AKVERIFY( pthread_mutex_lock( &m_mutexBlockingIO ) == 0 );

	// Now that we aquired the mutex, release the blocked thread. 
	AKASSERT( in_pWaitingTask->BlockedThreadID() != AK_NULL_THREAD );

	// Clear io_idBlockedThread: done inside locks to avoid race conditions with the client thread requesting
	// another blocking transfer.
	in_pWaitingTask->ClearBlockedStatus();

	// We cannot signal a specific thread. Thus, we signal all threads waiting for this variable. Only
	// the thread stuck in the task for which we cleared the BlockedStatus above will be released. Others will
	// go back to sleep.
	int eSignalResult = pthread_cond_broadcast( &m_condBlockingIODone );
	AKASSERT( eSignalResult == 0 );	
	
	AKVERIFY( pthread_mutex_unlock( &m_mutexBlockingIO ) == 0 );
}


#endif