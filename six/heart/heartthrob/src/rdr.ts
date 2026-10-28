import { Application, Text } from "pixi.js";
import * as _pen from "./pen";

// parameters
let _ctxt : Text;

(async () => {
  // Create a new application
  const app = new Application();

  // Initialize the application
  await app.init({ background: "#000e12", resizeTo: window });

  // add canvas
  document.getElementById('app')?.appendChild(app.canvas);

  // console
  _ctxt = new Text({ text: 'console', style: _pen._cost });
  _ctxt.anchor.set(0.0);
  _ctxt.position.set(5,5);
  app.stage.addChild(_ctxt);

  // first point ( need a daemon here )
  let _pt1 = _pen._crpt(150, 150, 1, 0xFFFFFF, "pmc")
  app.stage.addChild(_pt1)

  // initialize console record
  _pen._cstr_test()

  // daemon
  setInterval(() => {
    // _temon(_pt1)
    _upcost( _pen._cstr_get_all() )
  }, 1000);

})();

// update the console text accordingly
export function _upcost(_in: string): void {
    if (_in) {
        _ctxt.text = _in;
    }
}

// test console text
function _tecost( ): void {
    let _msg = "payload\n";
    for (let i =0; i < 100; i++)
    {
        _msg += "payload\n";
    }
    _upcost(_msg);
}

// test daemon
function _temon( _pti: ReturnType<typeof _pen._crpt> ): void {


    let _rx = Math.random() * (window.innerWidth - 20) + 25;
    let _ry = Math.random() * (window.innerHeight - 20) + 25;

    _pti.position.set(_rx, _ry);

}