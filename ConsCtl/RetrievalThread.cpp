//---------------------------------------------------------------------------
//
// RetrievalThread.cpp
//
// SUBSYSTEM: 
//              Monitoring process creation and termination  
//				
// MODULE:    
//              Provides an interface for handling queued items
//
// DESCRIPTION:
//
// AUTHOR:		Ivo Ivanov
//                                                                         
//---------------------------------------------------------------------------
#include "RetrievalThread.h"
#include "QueueContainer.h"

//---------------------------------------------------------------------------
//
// class CRetrievalThread
//
//---------------------------------------------------------------------------

CRetrievalThread::CRetrievalThread(
	TCHAR*            pszThreadGuid,
	CQueueContainer*  pQueue
	):
	CCustomThread(pszThreadGuid),
	m_pQueue(pQueue)
{
	assert( NULL != m_pQueue );
}

CRetrievalThread::~CRetrievalThread()
{

}


//
// A user supplied implementation of the thread function.
// Override Run() and insert the code that should be executed when 
// the thread runs.
//
void CRetrievalThread::Run()
{
	m_pQueue->WaitOnElementAvailable();
}

//----------------------------End of the file -------------------------------