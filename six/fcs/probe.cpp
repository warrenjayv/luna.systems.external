#include <string>
#include <iostream>
#include <Windows.h>

using namespace std;

HANDLE OpenProcessByID ( const DWORD id )
{
    return OpenProcess(PROCESS_ALL_ACCESS, false, id );
}

int main ( )
{
    int pID;
    cout << "please enter process id:";
    cin >> pID;

    cout << "open process initiated.";

    HANDLE proc = OpenProcessByID ( (DWORD) pID );

    return 0;
}