
#include "../header/di-mouse.h"
#include <dinput.h>

#pragma comment(lib, "dxguid")
#pragma comment(lib, "dinput8")

void di::mouse::test() 
{
    IDirectInput * _di = NULL;
  
    HRESULT hr = DirectInput8Create( GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput, (void**) &_di, NULL );
}


/*
```cpp
#include <dinput.h>

int main() {
  // create an idirectinput8 object
  idirectinput8* dinput;
  if (directinput8create(getmodulehandle(null), directinput_version, iid_idirectinput8, (void*)&dinput, null) != di_ok) {
    std::cerr << "failed to create idirectinput8 object" << std::endl;
    return 1;
  }

  // enumerate all connected input devices
  didataformat df;
  guid guid_joy = guid_joydev;
  iid iid_idirectinputdevice8 = iid_idirectinputdevice8;
  dinput->enum_devices(didevtype_gamepad, [](const dideviceinstance instance, void* context) -> bool {
    // create an idirectinputdevice8 object for the device
    idirectinputdevice8* device;
    if (dinput->create_device(instance->guid_instance, &device, null) != di_ok) {
      std::cerr << "failed to create idirectinputdevice8 object" << std::endl;
      return true;
    }

    // set the data format for the device
    if (device->set_data_format(&c_dfdi8gamepad) != di_ok) {
      std::cerr << "failed to set data format for device" << std::endl;
      return true;
    }

    // acquire the device so that it can be read from
    if (device->acquire() != di_ok) {
      std::cerr << "failed to acquire device" << std::endl;
      return true;
    }

    // read the state of the device
    didgamepadstate state;
    if (device->get_device_state(sizeof(state), &state) != di_ok) {
      std::cerr << "failed to get device state" << std::endl;
      return true;
    }

    // print the state of the device
    std::cout << "state of device " << instance->tszinstancename << ":" << std::endl;
    std::cout << "  buttons: " << state.rgbbuttons[0] << ", " << state.rgbbuttons[1] << ", " << state.rgbbuttons[2] << ", " << state.rgbbuttons[3] << std::endl
```
*/