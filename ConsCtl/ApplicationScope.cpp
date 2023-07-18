//---------------------------------------------------------------------------
//
// ApplicationScope.cpp
//
// SUBSYSTEM:   
//              Monitoring process creation and termination  
//				
// MODULE:      
//              Main interface of the user-mode app
//             
// DESCRIPTION: 
//              A class that wraps up different implementations and provide 
//              single interface
// 				
// AUTHOR:		Ivo Ivanov
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
// Includes
//
//---------------------------------------------------------------------------
#include "ApplicationScope.h"
#include "NtDriverController.h"
#include "ThreadMonitor.h"
#include "WinUtils.h"
#include "ThreadMonitor.h"
#include "QueueContainer.h"

//---------------------------------------------------------------------------
//
// class CApplicationScope 
//
//---------------------------------------------------------------------------

//
// Structure for holding info about activating/deactivating the driver
//
typedef struct _ActivateInfo
{
    BOOLEAN  bActivate;
} ACTIVATE_INFO, *PACTIVATE_INFO;


//---------------------------------------------------------------------------
//
// Static memeber declarations
//
//---------------------------------------------------------------------------
CApplicationScope* CApplicationScope::sm_pInstance = NULL;

//---------------------------------------------------------------------------
//
// Constructor
//
//---------------------------------------------------------------------------
CApplicationScope::CApplicationScope(
	CCallbackHandler* pHandler       // User-supplied object for handling notifications
	):
	m_pDriverCtl(NULL),
	m_bIsActive(FALSE),
	m_pProcessMonitor(NULL),
	m_pRequestManager(NULL)
{
	m_pRequestManager = new CQueueContainer(pHandler);	
	//
	// An instance of the class responsible for loading and unloading
	// the kernel driver
	//
	m_pDriverCtl = new CNtDriverController();
}

//---------------------------------------------------------------------------
//
// Destructor 
//
//---------------------------------------------------------------------------
CApplicationScope::~CApplicationScope()
{
	StopMonitoring();
	if (m_bIsActive)
		m_pDriverCtl->StopAndRemove();
	delete m_pDriverCtl;
	delete m_pRequestManager;
}

//---------------------------------------------------------------------------
//
// Copy constructor
//
//---------------------------------------------------------------------------
CApplicationScope::CApplicationScope(const CApplicationScope& rhs)
{

}

//---------------------------------------------------------------------------
//
// Assignment operator
//
//---------------------------------------------------------------------------
CApplicationScope& CApplicationScope::operator=(const CApplicationScope& rhs)
{
	if (this == &rhs) 
		return *this;

	return *this; // return reference to left-hand object
}



//---------------------------------------------------------------------------
// GetInstance
//
// Implements the "double-checking" locking pattern combined with 
// Scott Meyers single instance
// For more details see - 
// 1. "Modern C++ Design" by Andrei Alexandrescu - 6.9 Living in a 
//     Multithreaded World
// 2. "More Effective C++" by Scott Meyers - Item 26
//---------------------------------------------------------------------------
CApplicationScope& CApplicationScope::GetInstance(
	CCallbackHandler* pHandler       // User-supplied object for handling notifications
	) 
{
	VerifyIsWindowsNtRequired();
	if (!sm_pInstance)
	{
		CLockMgr<CCSWrapper> guard(g_AppSingeltonLock, TRUE);
		if (!sm_pInstance)
		{
			assert( NULL != pHandler );
			static CApplicationScope instance(pHandler);
			sm_pInstance = &instance;
		}
	} // if

	return *sm_pInstance;
}

//
// Activate/deactivate the monitoring process
//
BOOL CApplicationScope::SetActive(BOOL bActive)
{
	BOOL bResult     = FALSE;
	//
	// Verify the system hasn't been activate before
	//
	if (m_bIsActive != bActive)
	{
		BOOL           bReturnCode = FALSE;
		ACTIVATE_INFO  activateInfo;
		HANDLE         hDriverFile;
		DWORD          dwBytesReturned = 0;     // byte count

		if (bActive)
		{
			if (!m_pRequestManager->StartReceivingNotifications())
				return FALSE;
		}
		else
			m_pRequestManager->StopReceivingNotifications();
		//
		// Try opening the device driver
		//
		hDriverFile = ::CreateFile(
			TEXT("\\\\.\\ProcObsrv"),
			GENERIC_READ | GENERIC_WRITE, 
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			0,                     // Default security
			OPEN_EXISTING,
			0,                     // Perform synchronous I/O
			0);                    // No template
 		if (INVALID_HANDLE_VALUE == hDriverFile)
			return FALSE;
		//
		// and if the driver can be opened activate the monitoring
		// thread
		//
		if (NULL == m_pProcessMonitor)
		{
			m_pProcessMonitor = new CProcessThreadMonitor(
				TEXT("{30F8934F-F57F-4ced-93A6-AF68CD0F6E79}"), 
				m_pDriverCtl,
				m_pRequestManager
				);
			m_pProcessMonitor->SetActive( TRUE );
		}
		else
		{
			//
			// Stop and release the monitoring thread
			//
			delete m_pProcessMonitor;
			m_pProcessMonitor = NULL;
		}
		//
		// Inform the driver only when it has to be activated or
		// deactivated 
		//
		if (bActive)
		{
			//
			// Set input parameters for the driver routine
			//
			activateInfo.bActivate = bActive;
			//
			// Activate/Deactivate the process
			//
			bReturnCode = ::DeviceIoControl(
				hDriverFile,
				IOCTL_PROCOBSRV_ACTIVATE_MONITORING,
				&activateInfo, 
				sizeof(activateInfo),
				NULL, 
				0,
				&dwBytesReturned,
				NULL
				);
		} // if
		m_bIsActive = bActive;
		bResult   = TRUE;
		if (INVALID_HANDLE_VALUE != hDriverFile)
			::CloseHandle(hDriverFile);
	} // if
	return bResult;
}

//
// Initiates process of monitoring process creation/termination
//
BOOL CApplicationScope::StartMonitoring(
	PVOID pvParam        // Pointer to a parameter value passed to the object 
	)
{
	CLockMgr<CCSWrapper> guard(m_Lock, TRUE);
	BOOL bResult = FALSE;
	//
	// Verify the system hasn't been activate before
	//
	if (!m_bIsActive)
	{
		m_pRequestManager->SetExternalParam( pvParam );
		//
		// Activate the monitoring process
		//
		bResult = SetActive( TRUE ); 
	} // if
	
	return bResult;
}

//
// Ends up the whole process of monitoring
//
void CApplicationScope::StopMonitoring()
{
	CLockMgr<CCSWrapper> guard(m_Lock, TRUE);
	//
	// Deactivate the monitoring process
	//
	SetActive( FALSE );
	return;
}

//----------------------------End of the file -------------------------------
