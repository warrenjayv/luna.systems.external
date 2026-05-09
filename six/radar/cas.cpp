// come and see ( cas )

#include <iostream>
#include <sys/unistd.h>
#include <sstream>

#include "cas.cpp"

using namespace std; 

int main()
{
  int pID;
  cout << "please enter pid:";
  cin >> pID;
  cout << "pid: " << pID << endl;
  HANDLE proc = OpenProcessbyID( (DWORD) pid );
  return 0;
}






