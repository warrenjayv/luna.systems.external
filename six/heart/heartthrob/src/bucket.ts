const koffi = (window as any).require ? (window as any).require('koffi') : eval('require')('koffi');
import * as _pen from "./pen";

export function _attach( _target : number )
{
    const _k32 = koffi.load('kernel32.dll');
    const _psa = koffi.load('psapi.dll');

    const _HAND = koffi.pointer('HANDLE', koffi.opaque());
    const _HMOD = koffi.pointer('HMODULE', koffi.opaque());

    const OpenProcess = _k32.func('HANDLE __stdcall OpenProcess(uint32_t dwDesiredAccess, bool bInheritHandle, uint32_t dwProcessId)');
    const CloseHandle = _k32.func('bool __stdcall CloseHandle(HANDLE hObject)');
    const EnumProcessModules = _psa.func('bool __stdcall EnumProcessModules(HANDLE hProcess, _Out_ void *lphModule, uint32_t cb, _Out_ uint32_t *lpcbNeeded)');


    const _PAA = 0x1F0FFF;

    const _HPROC = OpenProcess(_PAA, false, _target);

    if (_HPROC)
    {
        _pen._cstr_add(3, "process attached.");
        const _HMODS = new Uint8Array(1024 * 8);
        let _CBN = 0;

        if (EnumProcessModules(_HPROC, _HMODS, 1024 * 8, _CBN))
        {
            const _BA = koffi.decode(_HMODS, 'uint64_t');
            _pen._cstr_add(4, 'base address: 0x${_BA.toString(16).toUpperCase()')
        } else
        {
            _pen._cstr_add(5, "failed to enumerate process modules.");
        }
        CloseHandle(_HPROC)
    } else 
    {
        console.log("process attach failure")
        _pen._cstr_add(6, "failed to open proccess.");
    }
}
