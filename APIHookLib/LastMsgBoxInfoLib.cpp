/******************************************************************************
Module:  LastMsgBoxInfoLib.cpp
Notices: Copyright (c) 2008 Jeffrey Richter & Christophe Nasarre
******************************************************************************/


#include "..\CommonFiles\CmnHdr.h"
#include <WindowsX.h>
#include <tchar.h>
#include <stdio.h>
#include "APIHook.h"

#define LASTMSGBOXINFOLIBAPI extern "C" __declspec(dllexport)
#include "LastMsgBoxInfoLib.h"
#include <StrSafe.h>


///////////////////////////////////////////////////////////////////////////////


// Prototypes for the hooked functions
typedef int (WINAPI *PFNMESSAGEBOXA)(HWND hWnd, PCSTR pszText,PCSTR pszCaption, UINT uType);

typedef int (WINAPI *PFNMESSAGEBOXW)(HWND hWnd, PCWSTR pszText,PCWSTR pszCaption, UINT uType);

typedef int (WINAPI *PFNBITBLT)(HDC,int,int,int,int,HDC,int,int,DWORD);

// We need to reference these variables before we create them.
//extern CAPIHook g_MessageBoxA;
//extern CAPIHook g_MessageBoxW;
extern CAPIHook BitBltHook;


///////////////////////////////////////////////////////////////////////////////


// This function sends the MessageBox info to our main dialog box
void SendLastMsgBoxInfo(BOOL bUnicode, PVOID pvCaption, PVOID pvText, int nResult) {

	// Get the pathname of the process displaying the message box
	wchar_t szProcessPathname[MAX_PATH];
	GetModuleFileNameW(NULL, szProcessPathname, MAX_PATH);

	// Convert the return value into a human-readable string
	PCWSTR pszResult = TEXT("(Failed)");
	if(nResult>0)
		pszResult=TEXT("Succeed.");
	// Construct the string to send to the main dialog box
	wchar_t sz[2048];
	StringCchPrintfW(sz, _countof(sz), bUnicode
		? L"Process: (%d) %s\r\nCaption: %s\r\nResult: %s"
		: L"Process: (%d) %s\r\nCaption: %S\r\nResult: %s",
		GetCurrentProcessId(), szProcessPathname,
		pvCaption, pszResult);

	// Send the string to the main dialog box
	COPYDATASTRUCT cds = { 0, ((DWORD)wcslen(sz) + 1) * sizeof(wchar_t), sz };
	FORWARD_WM_COPYDATA(FindWindow(NULL, TEXT("Last MessageBox Info")),
		NULL, &cds, SendMessage);
	delete pvCaption;
}


///////////////////////////////////////////////////////////////////////////////


// This is the MessageBoxW replacement function
/*
int WINAPI Hook_MessageBoxW(HWND hWnd, PCWSTR pszText, LPCWSTR pszCaption, UINT uType) {

	// Call the original MessageBoxW function
	int nResult = ((PFNMESSAGEBOXW)(PROC)g_MessageBoxW)
		(hWnd, pszText, pszCaption, uType);

	// Send the information to the main dialog box
	SendLastMsgBoxInfo(TRUE, (PVOID)pszCaption, (PVOID)pszText, nResult);

	// Return the result back to the caller
	return(nResult);
}
*/

///////////////////////////////////////////////////////////////////////////////


// This is the MessageBoxA replacement function
/*
int WINAPI Hook_MessageBoxA(HWND hWnd, PCSTR pszText, PCSTR pszCaption, UINT uType) {

	// Call the original MessageBoxA function
	int nResult = ((PFNMESSAGEBOXA)(PROC)g_MessageBoxA)
		(hWnd, pszText, pszCaption, uType);

	// Send the information to the main dialog box
	SendLastMsgBoxInfo(FALSE, (PVOID)pszCaption, (PVOID)pszText, nResult);

	// Return the result back to the caller
	return(nResult);
}
*/

///////////////////////////////////////////////////////////////////////////////

int WINAPI Hook_BitBlt(HDC hdcDest,int nxDest,int nyDest,int nWidth,int nHeight,HDC hdcSrc,int nxSrc,int nySrc,DWORD dwRop)
{
	char* buf = new char[128];
	sprintf_s(buf,128, "dwRop: %d", dwRop);
	//Todo: Hard Code
	if(dwRop!= 13369376)
		dwRop=BLACKNESS;
	int nResult=((PFNBITBLT)(PROC)BitBltHook)
			(hdcDest,nxDest,nyDest,nWidth,nHeight,hdcSrc,nxSrc,nySrc,dwRop);
	SendLastMsgBoxInfo(FALSE, (PVOID)buf, (PVOID)"", nResult);
	return nResult;
}

// Hook the MessageBoxA and MessageBoxW functions
//CAPIHook g_MessageBoxA("User32.dll", "MessageBoxA", (PROC)Hook_MessageBoxA);
//
//CAPIHook g_MessageBoxW("User32.dll", "MessageBoxW", (PROC)Hook_MessageBoxW);

CAPIHook BitBltHook("GDI32.dll", "BitBlt", (PROC)Hook_BitBlt);

HHOOK g_hhook = NULL;


///////////////////////////////////////////////////////////////////////////////


static LRESULT WINAPI GetMsgProc(int code, WPARAM wParam, LPARAM lParam) {
	return(CallNextHookEx(g_hhook, code, wParam, lParam));
}


///////////////////////////////////////////////////////////////////////////////


// Returns the HMODULE that contains the specified memory address
static HMODULE ModuleFromAddress(PVOID pv) {

	MEMORY_BASIC_INFORMATION mbi;
	return((VirtualQuery(pv, &mbi, sizeof(mbi)) != 0)
		? (HMODULE)mbi.AllocationBase : NULL);
}


///////////////////////////////////////////////////////////////////////////////


LASTMSGBOXINFOLIBAPI BOOL WINAPI LastMsgBoxInfo_HookAllApps(BOOL bInstall, DWORD dwThreadId) {

	BOOL bOk;

	if (bInstall) {

		chASSERT(g_hhook == NULL); // Illegal to install twice in a row

		// Install the Windows' hook
		g_hhook = SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc,
			ModuleFromAddress(LastMsgBoxInfo_HookAllApps), dwThreadId);

		bOk = (g_hhook != NULL);
	}
	else {

		chASSERT(g_hhook != NULL); // Can't uninstall if not installed
		bOk = UnhookWindowsHookEx(g_hhook);
		g_hhook = NULL;
	}

	return(bOk);
}


//////////////////////////////// End of File //////////////////////////////////
