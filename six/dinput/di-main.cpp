
#include "header/di-error.h"
#include "header/di-mouse.h"
#include "utilities/util-writer.h"
#include "utilities/util-linker.h"

int main ( )
{
     
    using util::linker;
    // linker::load_methods( linker::load("C:\\WINDOWS\\System32\\dinput8.dll"));

    di::mouse::test( );
  
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