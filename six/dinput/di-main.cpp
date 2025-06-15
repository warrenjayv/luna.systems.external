#include <dinput.h>

int main ( )
{
    IDirectInput8* di = NULL;
    IDirectInputDevice8* controller;

    HRESULT hr = DirectInput8Create( GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**) &di, NULL );
  
}