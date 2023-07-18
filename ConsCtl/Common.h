//---------------------------------------------------------------------------
//
// Common.h
//
// SUBSYSTEM:   
//              Monitoring process creation and termination  
//				
// DESCRIPTION: Common header 
//             
// AUTHOR:		Ivo Ivanov
//
//---------------------------------------------------------------------------
#if !defined(_COMMON_H_)
#define _COMMON_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//-----------------------Windows Version Build Option -----------------------


//---------------------------Unicode Build Option ---------------------------


//---------------------------------------------------------------------------
//
// Includes
//
//---------------------------------------------------------------------------
#include <windows.h>


//---------------------------------------------------------------------------
//
// struct _QueuedItem
//
//---------------------------------------------------------------------------
typedef struct _QueuedItem  
{
	DWORD32  hParentId;
    DWORD32  hProcessId;
    BOOLEAN bCreate;
} QUEUED_ITEM, *PQUEUED_ITEM;


#endif // !defined(_COMMON_H_)

//--------------------- End of the file -------------------------------------
