#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <Strsafe.h>
#include "../apihook/signatures.h"
using namespace std;
#define CONFIG_SAVE_FILE "save"
#define SIG_FILE "sig.dat"
#define TEMP_SIG_FILE "sigtemp.dat"
typedef BOOL (WINAPI *Wow64DisableWow64FsRedirectionFunc) ( __out PVOID *OldValue );
typedef BOOL (WINAPI *IsWow64ProcessFunc)(  __in   HANDLE hProcess,  __out  PBOOL Wow64Process);

//Key values
DWORD loadAppInit32 = 1, requireSignedAppInit32 = 0, loadAppInit64 = 1, requireSignedAppInit64 = 0;
char appinitDlls32[33000];
char appinitDlls64[33000];
char shortDllPath32[MAX_PATH];
char shortDllPath64[MAX_PATH];
HKEY winkey32 = 0, winkey64 = 0;
DWORD length = 4, length32 = 0, length64 = 0;
BOOL is64bit = TRUE;
//Exits with error message
void die(const char * message){
	cerr << message << GetLastError() << endl;
	Sleep(1000);
	exit(1);
}
//Adds a DLL path to an AppInit_DLLs value
void addShortPath(char * appInitVal, size_t appInitLen, DWORD &length, const char * shortDllPath){
	string appinitDllsString(appInitVal);
	size_t offset = appinitDllsString.find(shortDllPath);
	if(offset == appinitDllsString.npos){ //if we're not already there
		length--; // get rid of trailing null
		if(length > 0 && appInitVal[length-1] != ' ') //Add a space if needed
			appInitVal[length++] = ' ';
		strcpy_s(appInitVal + length, appInitLen - length, shortDllPath);
	}
}
//Removes a DLL path from an AppInit_DLLs value
void removeShortPath(char * appInitVal, const char * shortDllPath){
	string appinitDllsString(appInitVal);
	size_t offset = appinitDllsString.find(shortDllPath);
	size_t size = strlen(shortDllPath);
	if(offset == appinitDllsString.npos){
		cerr << "Did not find path in AppInit_Dlls\n";
		return;
	}
	if(offset > 0 && appinitDllsString.at(offset) == ' '){
		offset--;
		size++;
	}
	appinitDllsString.replace(offset, size, "");
	strcpy(appInitVal, appinitDllsString.c_str());
}
//Replaces registry values from save file and clears save file
void removeReg(){
	HANDLE outFile = CreateFileA(CONFIG_SAVE_FILE, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(outFile == INVALID_HANDLE_VALUE){
		cerr << "Cannot open save file!\n";
	}else{
		ReadFile(outFile, &loadAppInit32, sizeof(DWORD), &length, NULL);
		ReadFile(outFile, &requireSignedAppInit32, sizeof(DWORD), &length, NULL);
		if(is64bit){
			ReadFile(outFile, &loadAppInit64, sizeof(DWORD), &length, NULL);
			ReadFile(outFile, &requireSignedAppInit64, sizeof(DWORD), &length, NULL);
		}
		CloseHandle(outFile);
	}
	//Set for 64
	if(is64bit){
		RegSetValueExA(winkey64, "LoadAppInit_DLLs", 0, REG_DWORD, (PBYTE)&loadAppInit64, sizeof(DWORD));
		RegSetValueExA(winkey64, "RequireSignedAppInit_DLLs", 0, REG_DWORD, (PBYTE)&requireSignedAppInit64, sizeof(DWORD));
		//edit AppInit_DLLs
		length = sizeof(appinitDlls64);
		if(RegQueryValueExA(winkey64,"AppInit_DLLs",NULL,NULL,(PBYTE)appinitDlls64,&length) != ERROR_SUCCESS)
			die("Cannot get registry key");
		removeShortPath(appinitDlls64, shortDllPath64);
		RegSetValueExA(winkey64,"AppInit_DLLs", 0, REG_SZ, (PBYTE)appinitDlls64, strlen(appinitDlls64));
	}
	//Set for 32
	RegSetValueExA(winkey32, "LoadAppInit_DLLs", 0, REG_DWORD, (PBYTE)&loadAppInit32, sizeof(DWORD));
	RegSetValueExA(winkey32, "RequireSignedAppInit_DLLs", 0, REG_DWORD, (PBYTE)&requireSignedAppInit32, sizeof(DWORD));
	//edit AppInit_DLLs
	length = sizeof(appinitDlls32);
	if(RegQueryValueExA(winkey32, "AppInit_DLLs", NULL, NULL, (PBYTE)appinitDlls32, &length) != ERROR_SUCCESS)
		die("Cannot get registry key");
	removeShortPath(appinitDlls32, shortDllPath32);
	RegSetValueExA(winkey32,"AppInit_DLLs", 0, REG_SZ, (PBYTE)appinitDlls32, strlen(appinitDlls32));

	DeleteFileA(CONFIG_SAVE_FILE); //Delete config file
}
//Downloads a file from an internet connection
bool getFile(HINTERNET connection, LPCWCHAR path, const char * filename){
	HANDLE fileHandle = CreateFileA(filename,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,0);
	if(fileHandle == INVALID_HANDLE_VALUE){
		printf( "Error - download failed for %s Error %u\n", filename, GetLastError());
		return false;
	}

	// Create an HTTP Request handle.
	HINTERNET hRequest = NULL;
	if (connection)
		hRequest = WinHttpOpenRequest( connection, L"GET", path, 
				NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,0);
	BOOL  bResults = FALSE;
	if (hRequest){
		bResults = WinHttpSendRequest( hRequest, NULL, 0, NULL, 0, 0, 0);
	}else {
		printf( "Error - WinHttpSendRequest %u .\n", GetLastError());
		return false;
	}
	if (bResults){
		bResults = WinHttpReceiveResponse( hRequest, NULL);
	}else {
		printf( "Error - WinHttpReceiveResponse %u .\n", GetLastError());
		return false;
	}
	// Check status code
	DWORD dwStatusCode = 0;
	DWORD dwTemp = sizeof(dwStatusCode);
	bResults = WinHttpQueryHeaders( hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                 NULL, &dwStatusCode, &dwTemp, NULL );
	if(dwStatusCode != HTTP_STATUS_OK) {
		printf( "Error - HTTP status code %u .\n", dwStatusCode);
		return false;
	}
	if (!bResults){
		printf( "Error - WinHttpQueryHeaders %u .\n", GetLastError());
		return false;
	}

	//Read data
	DWORD numBytes;
	if (bResults){
		while(WinHttpQueryDataAvailable(hRequest, &numBytes) != FALSE && numBytes > 0){
			PCHAR outBuf = new char[numBytes];
			if (!outBuf){
				printf( "Error - out of memory\n");
				return false;
			} else {
				DWORD numDown;
				if (!WinHttpReadData( hRequest, (LPVOID)outBuf, numBytes, &numDown))
					printf( "Error %u in WinHttpReadData.\n", GetLastError());
				WriteFile(fileHandle, outBuf, numDown, &numBytes, NULL);
				delete [] outBuf;
			}
		}
	} else {
		printf( "Error %u in WinHttpReceiveResponse.\n", GetLastError());
		return false;
	}
	if (hRequest) WinHttpCloseHandle(hRequest);
	CloseHandle(fileHandle);
	return true;
}
//Downloads and verifies new signatures if possible
void doUpdate(){
	//Get config file
	HOOKAPI_CONF *apiConf;
	//Find configuration file from same binary directory as this file
	char filename[1000];
	DWORD size = GetModuleFileNameA(NULL, filename, sizeof(filename));
	for(size -= 1; filename[size] != '\\' && size != 0; size--)
		filename[size] = 0;
	SetCurrentDirectoryA(filename);
	cout << filename << endl;

	//Now download new config
	HINTERNET hSession = NULL,
	hConnect = NULL;
	hSession = WinHttpOpen(  L"Ambush IPS Client", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	//Get reporting server
	WCHAR server[257];
	CHAR serverc[257];
	size_t len = sizeof(serverc);
	DWORD length = 256;
	memset(serverc, 0, sizeof(serverc));
	HKEY settingsKey;
	RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Scriptjunkie\\Ambush", 0, KEY_QUERY_VALUE|KEY_WOW64_64KEY, &settingsKey);
	//Get server name/IP
	if(RegQueryValueExA(settingsKey, "SignatureServer", NULL, NULL, (PBYTE)serverc, &length) != ERROR_SUCCESS){
		cerr << "Could not find signature server " << GetLastError() << endl;
		return;
	}
	mbstowcs_s(&len, server, serverc, strlen(serverc));

	// Connect to the HTTP server.
	if (hSession){
		hConnect = WinHttpConnect( hSession, server, 3000, 0);//INTERNET_DEFAULT_HTTP_PORT
	}else {
		printf( "Error - WinHttpOpen %u .\n", GetLastError());
		return;
	}
	//Get signature set
	length = sizeof(DWORD);
	DWORD sigset;
	if(RegQueryValueExA(settingsKey, "SignatureSet", NULL, NULL, (PBYTE)&sigset, &length) != ERROR_SUCCESS)
		sigset = 1;
	WCHAR compiledURL[40];
	wsprintfW(compiledURL, L"/signature_sets/%d/compiled", sigset);
	WCHAR signedURL[40];
	wsprintfW(signedURL,  L"/signature_sets/%d/signature", sigset);
	if(!getFile(hConnect, compiledURL, "sigtemp.dat") 
			|| !getFile(hConnect, signedURL, "signed.dat"))
		return;

	//Check to see if we have a pubkey
	STARTUPINFOA start;
	PROCESS_INFORMATION proc;
	if(GetFileAttributesA("pub.key") == INVALID_FILE_ATTRIBUTES){
		if(!getFile(hConnect, L"/public.key", "pubtmp.key"))
			return;
		/*
		//The following code verifies the key validity based on a hash in the registry 
		//This may not be necessary, as the initial key pull should come during install, at 
		//which point we could assume the connection between the server and client is trusted.
		BYTE sha1reg[50];
		len = sizeof(sha1reg);
		if(RegQueryValueExA(settingsKey, "keyhash", NULL, NULL, sha1reg, &len) != ERROR_SUCCESS)
			return;
		//TODO: Get sig
		//popen openssl.exe dgst -sha1 pubtmp.key  or include sha1 code
		//TODO: compare sigs
		if(stricmp(sha1reg, hashval) != 0)
			return; //Bad pubkey
		//*/
		MoveFileA("pubtmp.key","pub.key"); //It's official! Install is complete; we have a key
	}

	//Verify signature
	memset(&start, 0, sizeof(start));
	memset(&proc, 0, sizeof(proc));
	start.cb = sizeof(start);
	if(CreateProcessA(NULL,"openssl.exe dgst -sha1 -verify pub.key -signature signed.dat sigtemp.dat",
			NULL,NULL,FALSE,0,NULL,NULL,&start,&proc)){
		WaitForSingleObject(proc.hProcess, 100000); //This shouldn't take that long
		DWORD exitCode;
		if(GetExitCodeProcess(proc.hProcess, &exitCode) && exitCode == 0){
			DeleteFileA(SIG_FILE);
			MoveFileA(TEMP_SIG_FILE, SIG_FILE);
		}else{
			printf( "Error - signature verification hung!\n");
			return; //File verification hung
		}
	}else {
		printf( "Error - signature verification failed!\n");
		return;
	}
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);
}
int main(int argc, char** argv){
	if(argc != 2)
		die("Api Hook config\nUsage: config [install|uninstall|update]");

	//Get command
	string command(argv[1]);
	//Open key (32)
	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
			0,KEY_QUERY_VALUE|KEY_SET_VALUE|KEY_WOW64_32KEY,&winkey32) != ERROR_SUCCESS && command.compare("update") != 0)
		die("Error opening key. Insufficient permissions?");
	//Get AppInit_DLLs values - might not exist
	if(RegQueryValueExA(winkey32,"LoadAppInit_DLLs",NULL,NULL,(PBYTE)&loadAppInit32,&length) != ERROR_SUCCESS)
		loadAppInit32 = 1;
	if(RegQueryValueExA(winkey32,"RequireSignedAppInit_DLLs",NULL,NULL,(PBYTE)&requireSignedAppInit32,&length) != ERROR_SUCCESS)
		requireSignedAppInit32 = 0;
	length = sizeof(appinitDlls32);
	if(RegQueryValueExA(winkey32,"AppInit_DLLs",NULL,NULL,(PBYTE)appinitDlls32,&length32) != ERROR_SUCCESS)
		length = 1;//make it a 0 length string
	//Check if 64 bit
	IsWow64ProcessFunc isWow = (IsWow64ProcessFunc)GetProcAddress(
		GetModuleHandleA("kernel32"),"IsWow64Process");
	if(isWow == NULL || isWow(GetCurrentProcess(), &is64bit) == 0)
		is64bit = FALSE;
	//Get 64 bit values
	//If is64
	PVOID OldValue;
	Wow64DisableWow64FsRedirectionFunc disableWow = (Wow64DisableWow64FsRedirectionFunc)GetProcAddress(
		GetModuleHandleA("kernel32"),"Wow64DisableWow64FsRedirection");
	if( disableWow )
		disableWow(&OldValue);
	if(is64bit){
		RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
				0,KEY_QUERY_VALUE|KEY_SET_VALUE|KEY_WOW64_64KEY,&winkey64);
		//Get AppInit_DLLs values for 64 bit
		if(RegQueryValueExA(winkey64,"LoadAppInit_DLLs",NULL,NULL,(PBYTE)&loadAppInit64,&length) != ERROR_SUCCESS)
			loadAppInit64 = 1;
		if(RegQueryValueExA(winkey64,"RequireSignedAppInit_DLLs",NULL,NULL,(PBYTE)&requireSignedAppInit64,&length) != ERROR_SUCCESS)
			requireSignedAppInit64 = 0;
		if(RegQueryValueExA(winkey64,"AppInit_DLLs",NULL,NULL,(PBYTE)appinitDlls64,&length64) != ERROR_SUCCESS)
			length = 1;//make it a 0 length string
	}

	//Change directory to binary directory
	char filename[MAX_PATH];
	DWORD size = GetModuleFileNameA(NULL, filename, sizeof(filename));
	for(size -= 1; filename[size] != '\\' && size != 0; size--)
		filename[size] = 0;
	SetCurrentDirectoryA(filename);

	//Get short path of DLLs
	string dllPath32(filename);
	string dllPath64(filename);
	dllPath32.append("apihook.dll");
	dllPath64.append("apihook64.dll");
	GetShortPathNameA(dllPath32.c_str(), shortDllPath32, sizeof(shortDllPath32));
	GetShortPathNameA(dllPath64.c_str(), shortDllPath64, sizeof(shortDllPath64));

	if(command.compare("install") == 0 || command.compare("/Commit") == 0){ //INSTALL
		doUpdate();

		//Save reg values
		if(GetFileAttributesA(CONFIG_SAVE_FILE) != INVALID_FILE_ATTRIBUTES)
			cerr << "Save file already exists\n";

		//Edit AppInit_DLLs if necessary
		addShortPath(appinitDlls32, sizeof(appinitDlls32), length32, shortDllPath32);
		addShortPath(appinitDlls64, sizeof(appinitDlls64), length64, shortDllPath64);

		//Write to file
		HANDLE outFile = CreateFileA(CONFIG_SAVE_FILE, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if(outFile == INVALID_HANDLE_VALUE)
			die("Cannot create save file");
		WriteFile(outFile,&loadAppInit32,sizeof(DWORD),&length,NULL);
		WriteFile(outFile,&requireSignedAppInit32,sizeof(DWORD),&length,NULL);
		if(is64bit){
			WriteFile(outFile,&loadAppInit64,sizeof(DWORD),&length,NULL);
			WriteFile(outFile,&requireSignedAppInit64,sizeof(DWORD),&length,NULL);
		}
		CloseHandle(outFile);

		//Set reg values
		DWORD zero = 0, one = 1;
		RegSetValueExA(winkey32,"LoadAppInit_DLLs", 0, REG_DWORD,(PBYTE)&one, sizeof(DWORD));
		RegSetValueExA(winkey32,"RequireSignedAppInit_DLLs", 0, REG_DWORD,(PBYTE)&zero, sizeof(DWORD));
		RegSetValueExA(winkey32,"AppInit_DLLs", 0, REG_SZ,(PBYTE)appinitDlls32, strlen(appinitDlls32));
		//If 64, disable wow registry and do it again
		if(is64bit){
			RegSetValueExA(winkey64, "LoadAppInit_DLLs", 0, REG_DWORD, (PBYTE)&one, sizeof(DWORD));
			RegSetValueExA(winkey64, "RequireSignedAppInit_DLLs", 0, REG_DWORD, (PBYTE)&zero, sizeof(DWORD));
			RegSetValueExA(winkey64, "AppInit_DLLs", 0, REG_SZ, (PBYTE)appinitDlls64, strlen(appinitDlls64));
		}
		//Schedule updates every four hours
		WCHAR cmdbuf[2000];
		WCHAR fn[2000];
		GetModuleFileNameW(NULL, fn, 2000);
		StringCbPrintfW(cmdbuf, 2000, 
			L"/c schtasks /create /tn AmbushSigUpdate /tr \"\\\"%s\\\" update\" /sc hourly /mo 4 /ru System", fn);
		
		STARTUPINFOW siStartupInfo;
		PROCESS_INFORMATION piProcessInfo;
		memset(&siStartupInfo, 0, sizeof(siStartupInfo));
		memset(&piProcessInfo, 0, sizeof(piProcessInfo));
		siStartupInfo.cb = sizeof(siStartupInfo);
		// set directory
		WCHAR dirbuf[2000];
		GetSystemDirectoryW(dirbuf,1999);
		SetCurrentDirectoryW(dirbuf);
		// run command
		if (CreateProcessW(L"cmd.exe", cmdbuf, 0, 0, FALSE, 0, 0, 0, &siStartupInfo, &piProcessInfo) == FALSE){
			removeReg(); //clean up
			die("Error scheduling update task.");
		}

	}else if(command.compare("uninstall") == 0 || command.compare("/Uninstall") == 0){ //UNINSTALL
		//Replace keys
		removeReg();

		//Clear scheduled task
		STARTUPINFOW siStartupInfo;
		PROCESS_INFORMATION piProcessInfo;
		memset(&siStartupInfo, 0, sizeof(siStartupInfo));
		memset(&piProcessInfo, 0, sizeof(piProcessInfo));
		siStartupInfo.cb = sizeof(siStartupInfo);
		WCHAR cmdline[1000];
		lstrcpyW(cmdline, L"/c schtasks /delete /tn AmbushSigUpdate /f");
		// set directory
		WCHAR dirbuf[2000];
		GetSystemDirectoryW(dirbuf,1999);
		SetCurrentDirectoryW(dirbuf);
		// run command
		if (CreateProcessW(L"cmd.exe", cmdline, 0, 0, FALSE, 0, 0, 0, &siStartupInfo, &piProcessInfo) == FALSE)
			cerr << "Error unscheduling update task.\n";
	}else if(command.compare("update") == 0){
		doUpdate();
		return 0;
	}else{
		die("Unknown command");
	}
	RegCloseKey(winkey32);
	RegCloseKey(winkey64);
}