
//\ const koffi = (window as any).require ? (window as any).require('koffi') : eval('require')('koffi');
import * as _pen from "./pen";

export function _attach(_target : number)
{
    //\ worker thread
    const _wkr = new Worker(new URL('./buc.work.ts', import.meta.url), {
        type: 'module'
    });

    //\ start scanner
    _wkr.postMessage({ type:'START_SCAN', target: _target});

    //\ listener(s)
    _wkr.onmessage = (event) => {
        const { type, code, msg } = event.data;
        if (type == 'LOG_INFO' || type == 'LOG_ERROR') {
            _pen._cstr_add(code, msg);
        }
    }
}

/*\
export function _attach_context( _target : number )
{
    //\ obtain koffi module
    const koffi = (window as any).require ? (window as any).require('koffi') : eval('require')('koffi');
    //\ import * as _pen from "./pen";

    //\ on message method
    //\ <-- body moved to buc.work.ts --> 

}
*///

