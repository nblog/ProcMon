//---------------------------------------------------------------------------
//
// CustomThread.cpp
//
// SUBSYSTEM: 
//              Monitoring process creation and termination  
// MODULE:    
//              Thread management 
//
// DESCRIPTION:
//              This is an abstract class that enables creation of separate 
//              threads of execution in an application. 
//
// AUTHOR:		Ivo Ivanov
//                                                                         
//---------------------------------------------------------------------------
#include "CustomThread.h"
#include <process.h>
#include <assert.h>

//---------------------------------------------------------------------------
//
// Thread function prototype
//
//---------------------------------------------------------------------------
typedef unsigned (__stdcall *PTHREAD_START) (void *);

//---------------------------------------------------------------------------
//
// class CCustomThread 
//
// It is an abstract class that enables creation of separate threads of 
// execution.
//                                                                         
//---------------------------------------------------------------------------

HANDLE CCustomThread::sm_hThread = NULL;

CCustomThread::CCustomThread(TCHAR* pszThreadGuid):
	m_hShutdownEvent(NULL),
	m_bThreadActive(NULL),
	m_dwThreadId(NULL)
{
	if (NULL != pszThreadGuid)
		_tcscpy(m_szThreadGuid, pszThreadGuid);
	else
		_tcscpy(m_szThreadGuid, TEXT(""));
}

CCustomThread::~CCustomThread()
{	
	SetActive( FALSE );
}

//
// Activate / Stop the thread 
//
void CCustomThread::SetActive(BOOL bValue)
{
	BOOL bCurrent = GetIsActive();

	if (bValue != bCurrent)
	{
		if (!bCurrent)
		{
			//
			// Perform action prior to activate the thread
			//
			if (!OnBeforeActivate())
				return;

			if (0 != _tcslen(m_szThreadGuid))
				m_hShutdownEvent = ::CreateEvent(NULL, FALSE, FALSE, m_szThreadGuid);
			ULONG ulResult = _beginthreadex(
				(void *)NULL,
				(unsigned)0,
				(PTHREAD_START)CCustomThread::ThreadFunc,
				(PVOID)this,
				(unsigned)0,
				(unsigned *)&m_dwThreadId
				);
			if (ulResult != -1)
				//
				// Wait until the thread gets activated
				//
				while (!GetIsActive())
				{
				}
		} 
		else
		{
			if ( GetIsActive() )
			{
				if (NULL != m_hShutdownEvent)
					//
					// Signal the thread's event
					//
					::SetEvent(m_hShutdownEvent);
				//
				// Wait until the thread is done
				//
				while (GetIsActive())
				{
				}
				//
				// Called after the thread function exits
				//
				OnAfterDeactivate();
			} // if
			if (NULL != m_hShutdownEvent)
				::CloseHandle(m_hShutdownEvent);
		}
	} // if
}

//
// Indicates whether the driver has been activated
//
BOOL CCustomThread::GetIsActive()
{
	CLockMgr<CCSWrapper> lockMgr(m_CritSec, TRUE);	
	return m_bThreadActive;
}

//
// Setup the attribute
//
void CCustomThread::SetIsActive(BOOL bValue)
{
	CLockMgr<CCSWrapper> lockMgr(m_CritSec, TRUE);	
	m_bThreadActive = bValue;
}


//
// Return the handle to the thread's shut down event
//
HANDLE CCustomThread::Get_ShutdownEvent() const
{
	return m_hShutdownEvent;
}

//
// Primary thread entry point
//
unsigned __stdcall CCustomThread::ThreadFunc(void* pvParam)
{
	CCustomThread* pMe = (CCustomThread*)( pvParam );
	// retrieves a pseudo handle for the current thread
	sm_hThread = GetCurrentThread();
	try
	{
		pMe->SetIsActive( TRUE );
		// Execute the user supplied method
		pMe->Run();
	}
	catch (...)
	{
		// Handle all exceptions
	}
	pMe->SetIsActive( FALSE );
	_endthreadex(0);
	return 0;
}


//
// Perform action prior to activate the thread
//
BOOL CCustomThread::OnBeforeActivate()
{
	// Provide default implementation
	return TRUE;
}

//
// Called after the thread function exits
//
void CCustomThread::OnAfterDeactivate()
{
	// Do nothing
}

//----------------------------End of the file -------------------------------