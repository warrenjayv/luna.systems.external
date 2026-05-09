// come and see ( cas )

#include <iostream>
#include <sys/unistd.h>
#include <sstream>
#include <Windows.h>
#include <memoryapi.h>
#include <psapi.h>
#include <cstdint>
#include "cas.cpp"

using namespace std; 

HANDLE OBID( const DWORD _ID )
{
  return OpenProcess(PROCESS_ALL_ACCESS, false, _ID);
}

int main()
{
  int pID;
  cout << "please enter pid:";
  cin >> pID;
  cout << "pid: " << pID << endl;
  HANDLE proc = OBID( (DWORD) pID );
  if (proc == NULL)
  {
    cout << "failed to attach" << endl;
  }
  return 0;
}






