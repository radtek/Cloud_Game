// InjectDll.cpp : Defines the entry point for the console application.
//

#include "stdio.h"
#include "stdafx.h"
#include <windows.h>

BOOL EnableDebugPriv()
{
	HANDLE hToken;
	LUID Luid;
	TOKEN_PRIVILEGES TokenPrivileges;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		printf("call OpenProcessToken failed");

		return FALSE;
	}

	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &Luid)) 
	{
		CloseHandle(hToken);
		printf("call LookupPrivilegeValue failed");

		return FALSE;
	}

	TokenPrivileges.PrivilegeCount = 1;
	TokenPrivileges.Privileges[0].Luid = Luid;
	TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!AdjustTokenPrivileges(hToken, FALSE, &TokenPrivileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
	{
		CloseHandle(hToken);
		printf("call AdjustTokenPrivileges failed");

		return FALSE;
	}

	CloseHandle(hToken);

	return TRUE;
}

BOOL
CreateApp(
	PPROCESS_INFORMATION pProcessInfo, 
	PWSTR pszFilePath, 
	PWSTR pszFileName
	)
{
	STARTUPINFO StartInfo = {NULL};				//启动窗口的信息
	StartInfo.cb = sizeof(STARTUPINFO);

	WCHAR szFile[MAX_PATH];
	swprintf(szFile, L"%s\\%s", pszFilePath, pszFileName);

	if (!CreateProcess(NULL, szFile, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, pszFilePath, &StartInfo, pProcessInfo))
		return FALSE;

	return TRUE;
}

BOOL
InjectDllW_NEW(
	HANDLE hProcess,
	PWSTR pszDllPath,
	PWSTR pszDllFile
	)
{
	HANDLE hThread = NULL;
	PWSTR pszLibFileRemote = NULL;	// 动态库内存地址

	WCHAR szDllName[MAX_PATH];
	swprintf(szDllName, L"%s\%s", pszDllPath, pszDllFile);

	__try
	{
		if (!EnableDebugPriv())
			MessageBox(NULL, L"提升权限失败！", L"test", 0);

		int iDllNameLen = (lstrlenW(szDllName) + 1) * sizeof(WCHAR);

		// 在指定进程划分内存地址，存在pLibAddr指针中
		pszLibFileRemote = (PWSTR)VirtualAllocEx(hProcess, NULL, iDllNameLen, MEM_COMMIT, PAGE_READWRITE);
		if (pszLibFileRemote == NULL)
		{
			MessageBox(NULL, L"获取虚拟内存地址失败！", L"test", 0);

			return FALSE;
		}

		// 将DLL文件地址写入到相关内存中
		if (!WriteProcessMemory(hProcess, pszLibFileRemote, (PVOID)szDllName, iDllNameLen,  NULL))
		{
			MessageBox(NULL, L"写进程内存失败！", L"test", 0);

			return FALSE;
		}

		// 得到LoadLibraryW函数物理地址，存放到pfnThreadRtn中。
		// 以上工作，纯粹是将要加载的DLL地址，COPY到目标进程的内存里， \
			然后用CreateRemoteThread加载运行LoadLibraryW函数，把它的数据作为参数加载进去。
		PTHREAD_START_ROUTINE pfnThreadRtn = (PTHREAD_START_ROUTINE) GetProcAddress(GetModuleHandle(TEXT("kernel32")), "LoadLibraryW");
		if (pfnThreadRtn == NULL)
		{
			MessageBox(NULL, L"GetProcAddress error！", L"test", 0);

			return FALSE;
		}

		// 创建一个在其它进程地址空间中运行的线程(也称:创建远程线程).
		hThread = CreateRemoteThread(hProcess, NULL, 0, pfnThreadRtn, pszLibFileRemote, 0, NULL);
		if (hThread == NULL)
		{
			DWORD dw = GetLastError(); 
			LPVOID lpMsgBuf;

			FormatMessageW(
					FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
					NULL,
					dw,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPTSTR)&lpMsgBuf,
					0,
					NULL);

			MessageBox(NULL, (LPCTSTR)lpMsgBuf, TEXT("Error"), MB_OK);

			LocalFree(lpMsgBuf);

			return FALSE;
		}

		// 使用该函数能够激活线程的运行，使CPU分配资源让线程恢复运行。该函数和SuspendThread(hthread)，线程挂起函数对应。
		ResumeThread(hThread);

		if (WAIT_FAILED == WaitForSingleObject(hThread, INFINITE))
		{
			MessageBox(NULL, L"call WaitForSingleObject failed！", L"test", 0);

			return FALSE;
		}
	}
	__finally
	{
		if (pszLibFileRemote != NULL)
		{
			if (!VirtualFreeEx(hProcess, pszLibFileRemote, 0, MEM_RELEASE))
			{
				puts("call VirtualFreeEx failed");
			}
		}

		if (hThread != NULL)
			CloseHandle(hThread);
	}

	return TRUE;
}

int _tmain(int argc, _TCHAR* argv[])
{
	PROCESS_INFORMATION ProcessInfo;

	WCHAR szBuf[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szBuf);
	wcscat(szBuf, L"\\");

	CreateApp(&ProcessInfo, szBuf, argv[1]);

	InjectDllW_NEW(ProcessInfo.hProcess, szBuf, argv[2]);

	ResumeThread(ProcessInfo.hThread);

	//system("pause");

	return 0;
}