//---------------------------------------------------------------------------
//
// ConsCtl.cpp
//
// SUBSYSTEM: 
//              Monitoring process creation and termination  
//
// MODULE:    
//				Control application for monitoring NT process and 
//              DLLs loading observation. 
//
// DESCRIPTION:
//
// AUTHOR:		Ivo Ivanov
//                                                                         
//---------------------------------------------------------------------------

#include <conio.h>
#include <tchar.h>
#include <Windows.h>
#include <Psapi.h>
#include "Common.h"
#include "ApplicationScope.h"
#include "CallbackHandler.h"

//
// This constant is declared only for testing putposes and
// determines how many process will be created to stress test
// the system
//
const int MAX_TEST_PROCESSES = 3;
//---------------------------------------------------------------------------
// 
// class CWhatheverYouWantToHold
//
// Implements a dummy class that can be used inside provide callback 
// mechanism. For example this class can manage sophisticated methods and 
// handles to a GUI Win32 Window. 
//
//---------------------------------------------------------------------------
class CWhatheverYouWantToHold
{
public:
	CWhatheverYouWantToHold()
	{
		_tcscpy(m_szName, TEXT("This could be any user-supplied data."));
		hwnd = NULL;
	}
private:
	TCHAR  m_szName[MAX_PATH];
	// 
	// You might want to use this attribute to store a 
	// handle to Win32 GUI Window
	//
	HANDLE hwnd;
};


//---------------------------------------------------------------------------
// 
// class CMyCallbackHandler
//
// Implements an interface for receiving notifications
//
//---------------------------------------------------------------------------
class CMyCallbackHandler: public CCallbackHandler
{
	//
	// Implements an event method
	//
	virtual void OnProcessEvent(
		PQUEUED_ITEM pQueuedItem, 
		PVOID        pvParam
		)
	{
		TCHAR szFileName[MAX_PATH];
		//
		// Deliberately I decided to put a delay in order to 
		// demonstrate the queuing / multithreaded functionality 
		//
		::Sleep(500);
		//
		// Get the dummy parameter we passsed when we 
		// initiated process of monitoring (i.e. StartMonitoring() )
		//
		CWhatheverYouWantToHold* pParam = static_cast<CWhatheverYouWantToHold*>(pvParam);
		//
		// And it's about time to handle the notification itself
		//
		if (NULL != pQueuedItem)
		{
			TCHAR szBuffer[1024];
			::ZeroMemory(
				reinterpret_cast<PBYTE>(szBuffer),
				sizeof(szBuffer)
				);
			/*
			GetProcessName(
				pQueuedItem->hProcessId, 
				szFileName, 
				MAX_PATH
				);
			*/

			HANDLE hProcess = OpenProcess(
				PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pQueuedItem->hProcessId);
			if (hProcess) {
				GetModuleFileNameEx(hProcess, NULL, szFileName, MAX_PATH);
				CloseHandle(hProcess);
			}

			if (pQueuedItem->bCreate)
				//
				// At this point you can use OpenProcess() and
				// do something with the process itself
				//
				wsprintf(
					szBuffer,
					TEXT("Process has been created: PID=0x%.8X %s\n"),
					pQueuedItem->hProcessId,
					szFileName
					);
			else
				wsprintf(
					szBuffer,
					TEXT("Process has been terminated: PID=0x%.8X\n"),
					pQueuedItem->hProcessId);
			//
			// Output to the console screen
			//
			_tprintf(szBuffer);
		} // if
	}
};

//---------------------------------------------------------------------------
// Perform
//
// Thin wrapper around __try..__finally
//---------------------------------------------------------------------------
void Perform(
	CCallbackHandler*        pHandler,
	CWhatheverYouWantToHold* pParamObject
	)
{
	DWORD processArr[MAX_TEST_PROCESSES] = {0};
	int i;
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	TCHAR szBuffer[MAX_PATH];  // buffer for Windows directory
	::GetWindowsDirectory(szBuffer, MAX_PATH);
	_tcscat(szBuffer, TEXT("\\notepad.exe"));

	//
	// Create the only instance of this object
	//
	CApplicationScope& g_AppScope = CApplicationScope::GetInstance(
		pHandler     // User-supplied object for handling notifications
		);
	__try
	{
		//
		// Initiate monitoring
		//
		g_AppScope.StartMonitoring(
			pParamObject // Pointer to a parameter value passed to the object 
			);
		for (i = 0; i < MAX_TEST_PROCESSES; i++)
		{
			// Spawn Notepad's instances
			if ( ::CreateProcess(NULL, szBuffer, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) 
			{
				::WaitForInputIdle(pi.hProcess, 1000);
				processArr[i] = pi.dwProcessId;
				::CloseHandle(pi.hThread);
				::CloseHandle(pi.hProcess);
			} // if
		} // for
		::Sleep(5000);
		for (i = 0; i < MAX_TEST_PROCESSES; i++)
		{
			HANDLE hProcess = ::OpenProcess(
				PROCESS_ALL_ACCESS, 
				FALSE,
				processArr[i]
				);
			if (NULL != hProcess)
			{
				__try
				{
					// Close Notepad's instances
					::TerminateProcess(hProcess, 0);
				}
				__finally
				{
					::CloseHandle(hProcess);
				}
			} // if
			::Sleep(10);
		} // for
	
		while(!kbhit())
		{
		}
		_getch();
	}
	__finally
	{
		//
		// Terminate the process of observing processes
		//
		g_AppScope.StopMonitoring();
	}
}

//---------------------------------------------------------------------------
// 
// Entry point
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	CMyCallbackHandler      myHandler;
	CWhatheverYouWantToHold myView; 

	Perform( &myHandler, &myView );

	return 0;
}
//--------------------- End of the file -------------------------------------
