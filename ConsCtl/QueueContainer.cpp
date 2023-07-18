//---------------------------------------------------------------------------
//
// QueueContainer.cpp
//
// SUBSYSTEM: 
//              Monitoring process creation and termination  
//				
// MODULE:    
//              Implement a multithreaded thread safe queue
// 
// DESCRIPTION:
//
// AUTHOR:		Ivo Ivanov
//                                                                         
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
// Includes
//
//---------------------------------------------------------------------------

#include "common.h"
#include "QueueContainer.h"

//---------------------------------------------------------------------------
//
// class CQueueContainer
//
//---------------------------------------------------------------------------

//
// Queue's constructor
//
CQueueContainer::CQueueContainer(CCallbackHandler* pHandler):
	m_pHandler(pHandler)
{
	Init();
}

//
// Queue's destructor
//
CQueueContainer::~CQueueContainer()
{
	Done();
}

//
// Get data from the kernel mode driver
//
void CQueueContainer::WaitOnElementAvailable()
{
	HANDLE handles[2] = 
	{
		m_evtShutdownRemove,
		m_evtElementAvailable
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
		if (handles[dwResult - WAIT_OBJECT_0] == m_evtShutdownRemove)
			break;
		//
		// An element just became available in the queue
		//
		else
		{
			DoOnProcessCreatedTerminated();
		}
	} // while
}

//
// A method for accessing handle to an internal event handle
//
HANDLE CQueueContainer::Get_ElementAvailableHandle() const
{
	return m_evtElementAvailable;
}

//
// Initates the process of handling notification in the queue
//
BOOL CQueueContainer::StartReceivingNotifications()
{
	BOOL bResult = FALSE;
	if (!m_pRetrievalThread->GetIsActive())
	{
		m_pRetrievalThread->SetActive( TRUE );
		bResult = m_pRetrievalThread->GetIsActive();
	}
	return bResult;
}

//
// Shutdown if there is something in progress
//
void CQueueContainer::StopReceivingNotifications()
{
	if (m_pRetrievalThread->GetIsActive())
		::SetEvent(m_evtShutdownRemove);
}

//
// Initialize the system
//
void CQueueContainer::Init()
{
	//
	// Create the monitoring mutex
	//
	m_mtxMonitor = ::CreateMutex(NULL, FALSE, NULL);
	assert(NULL != m_mtxMonitor);	//
	// Create the "remove" event
	//
	m_evtElementAvailable = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(NULL != m_evtElementAvailable);
	//
	// Create an event for shutting down the remove operation
	//
	m_evtShutdownRemove = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(NULL != m_evtShutdownRemove);
	//
	// Create a thread for picking up posted in the queue item notifications
	//
	m_pRetrievalThread = new CRetrievalThread(
		TEXT("{4EA19E49-1E3F-48da-AE16-2F2FD6A11F59}"),
		this
		);
}

//
// Release all allocated resources
//
void CQueueContainer::Done()
{
	StopReceivingNotifications();
	delete m_pRetrievalThread;
	if (NULL != m_evtElementAvailable)
		::CloseHandle(m_evtElementAvailable);
	if (NULL != m_evtShutdownRemove)
		::CloseHandle(m_evtShutdownRemove);
	if (NULL != m_mtxMonitor)
		::CloseHandle(m_mtxMonitor);
}
	
//
// Insert data into the queue
//
BOOL CQueueContainer::Append(const QUEUED_ITEM& element)
{
	BOOL bResult = FALSE;
	DWORD dw = ::WaitForSingleObject(m_mtxMonitor, INFINITE);
	bResult = (WAIT_OBJECT_0 == dw);
	if (bResult)
	{
		//
		// Add it to the STL queue
		//
		m_Queue.push_back(element);
		//
		// Notify the waiting thread that there is 
		// available element in the queue for processing 
		//
		::SetEvent(m_evtElementAvailable);
	}//
	::ReleaseMutex(m_mtxMonitor);
	return bResult;
}

//
// Implement specific behavior when kernel mode driver notifies 
// the user-mode app
//
void CQueueContainer::DoOnProcessCreatedTerminated()
{
	QUEUED_ITEM element;
	// Initially we have atleast one element for processing
	BOOL bRemoveFromQueue = TRUE;
	while (bRemoveFromQueue)
	{
		DWORD dwResult = ::WaitForSingleObject(
			m_mtxMonitor, INFINITE
			);
		if (WAIT_OBJECT_0 == dwResult)
		{
			bRemoveFromQueue = (m_Queue.size() > 0); 
			// Is there anything in the queue
			if (bRemoveFromQueue)
			{
				// Get the element from the queue
				element = m_Queue.front();	
				m_Queue.pop_front();
			} // if
			else
				//
				// Let's make sure that the event hasn't been 
				// left in signaled state if there are no items 
				// in the queue 
				//
				::ResetEvent(m_evtElementAvailable);
		} // if
		::ReleaseMutex(m_mtxMonitor);
		//
		// Process it only there is an element that has 
		// been picked up
		//
		if (bRemoveFromQueue)	
			m_pHandler->OnProcessEvent( &element, m_pvParam );
		else
			break;
	} // while
}

//
// Set an external parameter, thus we could take the advantage 
// of it later on in the callback routine
//
void CQueueContainer::SetExternalParam(PVOID pvParam)
{
	m_pvParam = pvParam;
}

//
// Delegate this method to a call of CCallbackHandler 
//
void CQueueContainer::OnProcessEvent(PQUEUED_ITEM pQueuedItem)
{
	if (pQueuedItem->bCreate)
		::OutputDebugString(TEXT("Process created.\n")); 
	else
		::OutputDebugString(TEXT("Process terminated.\n")); 

	m_pHandler->OnProcessEvent(
			pQueuedItem,
			m_pvParam
			);
}

//----------------------------End of the file -------------------------------