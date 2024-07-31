#include <string>
#include <iostream>
#include <Windows.h>
#include <memoryapi.h>
#include <psapi.h>

using namespace std;

HANDLE OpenProcessByID ( const DWORD id )
{
    return OpenProcess(PROCESS_ALL_ACCESS, false, id );
}

DWORD ListModules(const HANDLE hProcess)
{
    if( hProcess == NULL) { return NULL; }

    HMODULE lpModule[1024];
    DWORD   lpcbNeed;
    uint32_t i = 0;


    if (!EnumProcessModules(hProcess, lpModule, sizeof(lpModule), &lpcbNeed))
        return (DWORD)NULL;
    
    uint32_t len = lpcbNeed / sizeof(HMODULE);

    for(i = 0; i < len; i++)
    {
        if ( i >= len ) { break; }
        TCHAR modName[MAX_PATH];
        if(!GetModuleFileNameEx(hProcess, lpModule[i], modName, sizeof(modName)/sizeof(TCHAR)))
        {
            cout << "error!";
        }
        wcout << modName << endl;
    }
    

}

int main ( )
{
    int pID;
    cout << "please enter process id:";
    cin >> pID;

    cout << "open process initiated." << endl;

    HANDLE proc = OpenProcessByID ( (DWORD) pID );

    // ListModules(proc);

    cin >> pID;

    return 0;
}