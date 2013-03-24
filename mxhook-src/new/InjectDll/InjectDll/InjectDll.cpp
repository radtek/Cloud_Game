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
	STARTUPINFO StartInfo = {NULL};				//�������ڵ���Ϣ
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
	PWSTR pszLibFileRemote = NULL;	// ��̬���ڴ��ַ

	WCHAR szDllName[MAX_PATH];
	swprintf(szDllName, L"%s\%s", pszDllPath, pszDllFile);

	__try
	{
		if (!EnableDebugPriv())
			MessageBox(NULL, L"����Ȩ��ʧ�ܣ�", L"test", 0);

		int iDllNameLen = (lstrlenW(szDllName) + 1) * sizeof(WCHAR);

		// ��ָ�����̻����ڴ��ַ������pLibAddrָ����
		pszLibFileRemote = (PWSTR)VirtualAllocEx(hProcess, NULL, iDllNameLen, MEM_COMMIT, PAGE_READWRITE);
		if (pszLibFileRemote == NULL)
		{
			MessageBox(NULL, L"��ȡ�����ڴ��ַʧ�ܣ�", L"test", 0);

			return FALSE;
		}

		// ��DLL�ļ���ַд�뵽����ڴ���
		if (!WriteProcessMemory(hProcess, pszLibFileRemote, (PVOID)szDllName, iDllNameLen,  NULL))
		{
			MessageBox(NULL, L"д�����ڴ�ʧ�ܣ�", L"test", 0);

			return FALSE;
		}

		// �õ�LoadLibraryW����������ַ����ŵ�pfnThreadRtn�С�
		// ���Ϲ����������ǽ�Ҫ���ص�DLL��ַ��COPY��Ŀ����̵��ڴ�� \
			Ȼ����CreateRemoteThread��������LoadLibraryW������������������Ϊ�������ؽ�ȥ��
		PTHREAD_START_ROUTINE pfnThreadRtn = (PTHREAD_START_ROUTINE) GetProcAddress(GetModuleHandle(TEXT("kernel32")), "LoadLibraryW");
		if (pfnThreadRtn == NULL)
		{
			MessageBox(NULL, L"GetProcAddress error��", L"test", 0);

			return FALSE;
		}

		// ����һ�����������̵�ַ�ռ������е��߳�(Ҳ��:����Զ���߳�).
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

		// ʹ�øú����ܹ������̵߳����У�ʹCPU������Դ���ָ̻߳����С��ú�����SuspendThread(hthread)���̹߳�������Ӧ��
		ResumeThread(hThread);

		if (WAIT_FAILED == WaitForSingleObject(hThread, INFINITE))
		{
			MessageBox(NULL, L"call WaitForSingleObject failed��", L"test", 0);

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

	CreateApp(&ProcessInfo, szBuf, L"test.exe");

	InjectDllW_NEW(ProcessInfo.hProcess, szBuf, L"HookDll.dll");

	ResumeThread(ProcessInfo.hThread);

	//system("pause");

	return 0;
}