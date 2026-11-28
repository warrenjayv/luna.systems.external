import { Text, TextStyle, Graphics, Container } from "pixi.js";
import type { PointPair } from "./network.work";
export type { PointPair };

const POINT_RADIUS = 2;

// console structure
let _cstr: Record<number, string> = {};

// drawn address-pair point containers keyed by pair id
const _drawnPoints = new Map<string, Container>();

// console style
export const _cost = new TextStyle({
  fontFamily: "Arial",
  fontSize: 10,
  fontStyle: "italic",
  fontWeight: "normal",
  fill: "#ffffff",
  wordWrap: true,
  wordWrapWidth: 440,
});

// create point instance
export function _crpt(
  _x: number,
  _y: number,
  _r: number,
  _hc: number,
  _lb: string,
): Container {
  const _cr = new Container();
  const _cg = new Graphics();
  const _el = new Text({ text: _lb, style: _cost });

  _cg.circle(0, 0, _r);
  _cg.fill({ color: _hc });

  _el.anchor.set(0.5, 1.0);
  _el.position.set(0, -_r - 2);

  _cr.addChild(_cg);
  _cr.addChild(_el);

  _cr.position.set(_x, _y);

  return _cr;
} //\ _crpt

// draw or update all closest-address point pairs on the target stage
export function _drawPoints(
  stage: Container,
  pairs: readonly PointPair[],
  width: number,
  height: number,
): void {
  const activeIds = new Set<string>();

  for (const pt of pairs) {
    activeIds.add(pt.id);

    const x = Math.max(16, Math.min(width - 16, pt.x));
    const y = Math.max(16, Math.min(height - 16, pt.y));

    let dot = _drawnPoints.get(pt.id);
    if (!dot) {
      dot = _crpt(x, y, POINT_RADIUS, 0x00e5ff, `${pt.addrA} / ${pt.addrB}`);
      stage.addChild(dot);
      _drawnPoints.set(pt.id, dot);
    } else {
      dot.position.set(x, y);
    }
  }

  for (const [id, container] of _drawnPoints.entries()) {
    if (!activeIds.has(id)) {
      stage.removeChild(container);
      container.destroy({ children: true });
      _drawnPoints.delete(id);
    }
  }
} //\ _drawPoints

export function _cstr_add(_i: number, _v: string): void {
  _cstr[_i] = _v;
}

export function _cstr_get_all(): string {
  let _tmp = "";
  for (const key in _cstr) {
    _tmp += _cstr[key] + "\n";
  }
  return _tmp;
}

export function _cstr_test(): void {
  _cstr = {
    0: "console initalized.\n",
    1: "ready to populate.\n",
    2: "...",
  };
}