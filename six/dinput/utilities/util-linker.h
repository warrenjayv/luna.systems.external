#ifndef   UTIL_LINK_H
#define   UTIL_LINK_H

#include <windows.h>
#include <iostream>
#include <string>

#include "util-writer.h"


namespace util
{
   
  class linker
  {
    public:
      typedef int (*di8dll)(); 
      
      static di8dll *di8_create;
      
      static HMODULE load(char* dllpath)
      {
        wchar_t wide_dll[MAX_PATH];
        mbstowcs(wide_dll, dllpath, strlen(dllpath) + 1);

        HMODULE hmod = LoadLibraryExW(wide_dll, NULL, 0);
        if ( hmod == NULL )
        {
           aut::write("linker: unable to load dll: ", (char *)dllpath, col::red );
           return hmod;
        } 
        
        return hmod;
      }

      static int load_methods( HMODULE hmod )
      {
        if (hmod == NULL) { return -1; }
      
        di8_create =  (di8dll*) GetProcAddress(hmod, "DirectInput8Create");
        status(di8_create);
        
      }

      static int status(di8dll *func )
      {
      
        if ( func != NULL)
        {
           aut::write("linker::load()->", (char *)func, col::green);
           return 0;
        }
        else
        {
          aut::write("linker::load()-> fail! <NULL> method", col::red);
          return -1;
        }

      }
  };

  util::linker::di8dll* util::linker::di8_create = nullptr;



}

#endif