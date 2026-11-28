import { Application, Text } from "pixi.js";
import * as _pen from "./pen";
import * as _buc from "./bucket";
import { _net } from "./network";

// worker threads

// parameters
let _ctxt: Text;

(async () => {
  // Create a new application
  const app = new Application();

  // Initialize the application
  await app.init({ background: "#000e12", resizeTo: window });

  // add canvas
  document.getElementById("pixi-container")?.appendChild(app.canvas);

  // console
  _ctxt = new Text({ text: "console", style: _pen._cost });
  _ctxt.anchor.set(0.0);
  _ctxt.position.set(5, 5);
  app.stage.addChild(_ctxt);

  // first point ( need a daemon here )
  let _pt1 = _pen._crpt(150, 150, 1, 0xffffff, "pmc");
  app.stage.addChild(_pt1);

  // initialize console record
  _pen._cstr_test();

  // attach to target process
  // fixme: moved to another tread
  try {
    setTimeout(() => {
      try {
        _net.onStatus((state, detail) => {
          _pen._cstr_add(1, `net[${state}]${detail ? `: ${detail}` : ""}`);
        });

        _net.onSamples((items) => {
          for (const s of items) {
            _pen._cstr_add(0, `${s.address} -> ${s.value.toFixed(4)} (f32)`);
          }
        });

        _net.onPoints((pairs) => {
          _pen._drawPoints(
            app.stage,
            pairs,
            app.screen.width,
            app.screen.height,
          );
        }); 

        // _buc._attach(1164);
        _net.start("127.0.0.1", 9000, app.screen.width, app.screen.height);

        // viewport resize
        const handleResize = () => {
          _net.setViewport(app.screen.width, app.screen.height);
        };
        window.addEventListener("resize", handleResize);

      } catch (_err: unknown) {
        const msg = _err instanceof Error ? _err.message : String(_err);
        _pen._cstr_add(4, `error: ${msg}`);
        console.error("failure: ", _err);
      }
    }, 100);
  } catch (_err) {
    _pen._cstr_add(4, `error Occurred:\n ${_err}`);
    console.log("failure detected:", _err);
  } 

  // daemon
  setInterval(() => {
    // _temon(_pt1)
    _upcost(_pen._cstr_get_all());
  }, 1000);
})(); 

// update the console text accordingly
export function _upcost(_in: string): void {
  if (_in) {
    _ctxt.text = _in;
  }
}

// test console text
/*
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

}*/
