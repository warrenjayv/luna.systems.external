#include <dinput.h>
#include <initguid.h>

#include "header/di-error.h"
#include "utilities/util-writer.h"
#include "utilities/util-linker.h"

#pragma comment(lib, "dxguid")
#pragma comment(lib, "dinput8")

int main ( )
{
     
    using util::linker;
    linker::load("C:\\WINDOWS\\System32\\dinput.dll");

    IDirectInput* di = NULL;
    IDirectInputDevice* controller;

    HRESULT hr = DirectInput8Create( GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput, (void**) &di, NULL );
  
    using util::aut; using util::col;
    // aut::write((char*)"test", col::red );

    while ( true ) { }
}

/*
#include <windows.h>
#include <iostream>

int main() {
    hmodule hmodule = loadlibrary("mydll.dll");
    if (hmodule == null) {
        std::cerr << "could not load dll" << std::endl;
        return 1;
    }

    typedef void (*myfunction)();
    myfunction function = (myfunction)getprocaddress(hmodule, "myfunction");
    if (function == null) {
        std::cerr << "could not find function" << std::endl;
        freelibrary(hmodule);
        return 1;
    }

    function();

    freelibrary(hmodule);
    return 0;
}*/