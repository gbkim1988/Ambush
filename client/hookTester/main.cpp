#include <windows.h>

typedef HANDLE (WINAPI *URLDownloadToFileAFunc)(
    LPUNKNOWN pCaller,
    LPCSTR szURL,
    LPCSTR szFileName,
    DWORD dwReserved,
    LPBINDSTATUSCALLBACK lpfnCB
);
DWORD WINAPI ThreadProc(LPVOID arg){
	MessageBoxA(NULL,"I am in a new thread!","Hook Tester",0);
	return 0;
}
int CALLBACK WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
	if(MessageBoxA(NULL,"Manually load library?","Hook Tester",MB_YESNO) == IDYES)
		if(LoadLibraryA("apihook.dll") == NULL)
			MessageBoxA(NULL,"Load failed!","Hook Tester",0);
	//Basic test
	if(MessageBoxA(NULL,"Test starting. SleepEx 1000...","Hook tester",MB_YESNO) != IDNO)
		SleepEx(1000, FALSE);
	//Block test
	if(MessageBoxA(NULL,"SleepEx 1001","Hook tester",MB_YESNO) != IDNO)
		SleepEx(1001, FALSE);
	//Test LoadLibrary, GetProcAddress
	MessageBoxA(NULL,"LoadLibraryA","Hook tester",0);
	URLDownloadToFileAFunc URLDownloadToFileA = (URLDownloadToFileAFunc)
		GetProcAddress(LoadLibraryA("urlmon"), "URLDownloadToFileA");
	if(MessageBoxA(NULL,"URLDownloadToFileA http://yahoo.com/", "Hook tester", MB_YESNO) != IDNO)
		URLDownloadToFileA(NULL, "http://yahoo.com/", "deleteme.txt", 0, NULL);

	STARTUPINFOW start;
	PROCESS_INFORMATION proc;
	memset(&start,0,sizeof(start));
	memset(&proc,0,sizeof(proc));
	wchar_t cmdline[100];
	lstrcpyW(cmdline,L"calc");
	if(MessageBoxA(NULL,"CreateProcessW calc", "Hook tester", MB_YESNO) != IDNO)
		CreateProcessW(NULL,cmdline,NULL,NULL,0,0,NULL,NULL,&start,&proc);
	if(MessageBoxA(NULL,"WinExec calc", "Hook tester", MB_YESNO) != IDNO)
		WinExec("calc",0);
	//Test killproc with sleepEx 1002
	if(MessageBoxA(NULL,"SleepEx 1002", "Hook tester", MB_YESNO) != IDNO)
		SleepEx(1002, FALSE);
	if(MessageBoxA(NULL,"Non-remote CreateRemoteThread", "Hook tester", MB_YESNO) != IDNO)
		CreateRemoteThread(GetCurrentProcess(),NULL,0,&ThreadProc,NULL,0,NULL);
	MessageBoxA(NULL,"Test complete","Hook tester",0);
}