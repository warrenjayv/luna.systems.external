#ifndef   UTIL_LINK_H
#define   UTIL_LINK_H

#include <windows.h>
#include <iostream>

#include "util-writer.h"


namespace util
{
  // return flags
  static const int LUERR = -1;
  static const int LUGUD =  0;

  class linker
  {
    public:
      static int load(char* dllpath)
      {
        HMODULE hmod = LoadLibraryEx((const CHAR*)dllpath, NULL, 0);
        if ( hmod == NULL )
        {
           aut::write("linker: unable to load dll: ", dllpath, col::red );
           return LUERR;
        } 
        
        return LUGUD;
      }
  };
/* #include <windows.h>

int main() {
    wchar_t* dll_name = L"mydll.dll";
    hmodule hmodule = loadlibraryex(dll_name, null, loadlibraryex);
    return 0;
    */

}

#endif