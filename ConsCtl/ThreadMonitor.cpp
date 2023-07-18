//---------------------------------------------------------------------------
//
// ThreadMonitor.cpp
//
// SUBSYSTEM: 
//              Monitoring process creation and termination  
//				
// MODULE:    
//              Thread management
//
// DESCRIPTION:
//              Implement abstract interface provided by CCustomThread
//
// AUTHOR:		Ivo Ivanov
//                                                                         
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
//
// Includes
//
//---------------------------------------------------------------------------
#include "ThreadMonitor.h"
#include "NtDriverController.h"

//---------------------------------------------------------------------------
//
// class CThreadMonitor
//
//---------------------------------------------------------------------------
CProcessThreadMonitor::CProcessThreadMonitor(
	TCHAR*               pszThreadGuid,     // Thread unique ID
	CNtDriverController* pDriverController, // service controller
	CQueueContainer*     pRequestManager    // The underlying store
	):
	CCustomThread(pszThreadGuid),
	m_pRequestManager(pRequestManager),
	m_hKernelEvent(INVALID_HANDLE_VALUE),
	m_hDriverFile(INVALID_HANDLE_VALUE)
{
	assert(NULL != pDriverController);
	//
	// Store the pointe to the service controller, thus
	// we can use it later
	//
	m_pDriverCtl = pDriverController;

	::ZeroMemory((PBYTE)&m_LastCallbackInfo, sizeof(m_LastCallbackInfo));
}

CProcessThreadMonitor::~CProcessThreadMonitor()
{

}

//
// A user supplied implementation of the thread function.
// Override Run() and insert the code that should be executed when 
// the thread runs.
//
void CProcessThreadMonitor::Run()
{
	HANDLE handles[2] = 
	{
		m_hShutdownEvent,
		m_hKernelEvent
	};

	while (TRUE)
	{
		DWORD dwResult = ::WaitForMultipleObjects(
			sizeof(handles)/sizeof(handles[0]), // number of handles in array
			&handles[0],                        // object-handle array
			FALSE,                              // wait option
			INFINITE                            // time-out interval
			);
		//
		// the system shuts down
		//
		if (handles[dwResult - WAIT_OBJECT_0] == m_hShutdownEvent)
			break;
		//
		// The kernel event has been just signaled
		//
		else
			RetrieveFromKernelDriver();
	} // while
}

//
// Perform action prior to activate the thread
//
BOOL CProcessThreadMonitor::OnBeforeActivate()
{
	BOOL bResult = FALSE;
	//
	// Try opening the device driver
	//
	m_hDriverFile = ::CreateFile(
		TEXT("\\\\.\\ProcObsrv"),
		GENERIC_READ | GENERIC_WRITE, 
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		0,                     // Default security
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,  // Perform asynchronous I/O
		0);                    // No template
	if (INVALID_HANDLE_VALUE != m_hDriverFile)
		//
		// Attach to kernel mode created event handle
		//
		bResult = OpenKernelModeEvent();

	return bResult;
}

//
// Called after the thread function exits
//
void CProcessThreadMonitor::OnAfterDeactivate()
{
	if (INVALID_HANDLE_VALUE != m_hKernelEvent)
	{
		::CloseHandle(m_hKernelEvent);
		m_hKernelEvent = INVALID_HANDLE_VALUE;
	}
	if (INVALID_HANDLE_VALUE != m_hDriverFile)
	{
		::CloseHandle(m_hDriverFile);
		m_hDriverFile = INVALID_HANDLE_VALUE;
	}
}

//
// Attach to the particular kernel mode created event handle
//
BOOL CProcessThreadMonitor::OpenKernelModeEvent()
{
	// https://learn.microsoft.com/windows/win32/termserv/kernel-object-namespaces
	m_hKernelEvent = ::OpenEvent(
		SYNCHRONIZE, FALSE, 
#if ( _WIN32_WINNT < 0x0600 /*_WIN32_WINNT_VISTA*/ )
	TEXT("ProcObsrvProcessEvent")
#else
	TEXT("Global\\ProcObsrvProcessEvent")
#endif
	);

	return (INVALID_HANDLE_VALUE != m_hKernelEvent);
}

//
// Retrieve data from the kernel mode driver.
//
void CProcessThreadMonitor::RetrieveFromKernelDriver()
{
	OVERLAPPED  ov          = { 0 };
	BOOL        bReturnCode = FALSE;
	DWORD       dwBytesReturned;
	PROCESS_CALLBACK_INFO  callbackInfo;
	QUEUED_ITEM queuedItem;         
	//
    // Create an event handle for async notification from the driver
	//
	ov.hEvent = ::CreateEvent(
		NULL,  // Default security
		TRUE,  // Manual reset
		FALSE, // non-signaled state
		NULL
		); 
	//
	// Get the process info
	//
	bReturnCode = ::DeviceIoControl(
		m_hDriverFile,
		IOCTL_PROCOBSRV_GET_PROCINFO,
		0, 
		0,
		&callbackInfo, sizeof(callbackInfo),
		&dwBytesReturned,
		&ov
		);
	//
	// Wait here for the event handle to be set, indicating
	// that the IOCTL processing is completed.
	//
	bReturnCode = ::GetOverlappedResult(
		m_hDriverFile, 
		&ov,
		&dwBytesReturned, 
		TRUE
		);
	//
	// Prevent duplicated events
	//
	if ( (m_LastCallbackInfo.bCreate != callbackInfo.bCreate) ||
	     (m_LastCallbackInfo.hParentId != callbackInfo.hParentId) ||
		 (m_LastCallbackInfo.hProcessId != callbackInfo.hProcessId) )
	{
		//
		// Setup the queued element
		//
		::ZeroMemory((PBYTE)&queuedItem, sizeof(queuedItem));
		queuedItem.hParentId = callbackInfo.hParentId;
		queuedItem.hProcessId = callbackInfo.hProcessId;
		queuedItem.bCreate = callbackInfo.bCreate;
		//
		// and add it to the queue
		//
		m_pRequestManager->Append(queuedItem);
		//
		// Hold last event
		//
		m_LastCallbackInfo = callbackInfo;
	} // if
	//
	//
	//
	::CloseHandle(ov.hEvent);
}

//----------------------------End of the file -------------------------------
