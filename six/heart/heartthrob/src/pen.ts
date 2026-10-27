import { Text, Assets, Sprite, TextStyle, Graphics, Container } from "pixi.js";

// console style
export const _cost = new TextStyle({
    fontFamily: 'Arial',
    fontSize: 10,
    fontStyle: 'italic',
    fontWeight: 'normal',
    fill: '#ffffff',
    // stroke: { color: '#4a1850', width: 5, join: 'round' }, 
    /* dropShadow: {
        color: '#000000',
        blur: 4,
        angle: Math.PI / 6,
        distance: 6,
    }, */
    wordWrap: true,
    wordWrapWidth: 440,
});

 // create point instance
export function _crpt(_x: number, _y: number, _r: number, _hc: number, _lb: string):Container
{
    const _cr = new Container();
    const _cg = new Graphics();
    const _el = new Text({ text: _lb, style: _cost })

    _cg.circle(0, 0, _r);
    _cg.fill({ color: _hc });

    _el.anchor.set(0.5, 1.0);
    _el.position.set(0, _r - 2);

    _cr.addChild(_cg);
    _cr.addChild(_el);
    
    _cr.position.set(_x,_y)

    return _cr;
}


