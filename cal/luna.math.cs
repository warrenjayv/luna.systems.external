using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace luna {
    namespace cal {
 
        // <optimized out> - .net trig - 01/12/23 13:56
        public static class trig {
            public static float r2d = (float) (180.0 / Math.PI);
            public static float d2r = (float) (Math.PI / 180.0);
            public static float hpi = (float) (Math.PI / 2.0); 
            public static float wpi = (float) (Math.PI * 2.0);
            public static float qpi = (float) (Math.PI / 2.0);
            public static float tpi = (float) (Math.PI * 3.0 / 2.0f); 
            public static float crs = (float) (( Math.PI * 2.0) / 360.0); 

            public delegate double   _cos ( double c );
            public delegate double _acos ( double c );
            public delegate double   _sin ( double c );
            public delegate double _atan ( double c ); 

            public static _cos     cos  = new  _cos    ( Math.Cos );
            public static _acos  acos = new  _acos   ( Math.Acos );
            public static _sin      sin = new  _sin     ( Math.Sin );
            public static _atan  atan  = new _atan  ( Math.Atan); 

        }

        public static class nota { 
            public static float ten3 = (float) Math.Pow(10, 3);
             
        }

        public static class vector {

            // <optimized out> - delegate cos sin 
            public delegate double _cos ( double c );
            public delegate double _sin ( double c );

            public static _cos cos = new _cos (  Math.Cos );
            public static _sin sin = new _sin  ( Math.Sin );


            // <optimized out> - vector add - 2022-12-15 09:52:42
            public static _vector add ( _vector a,  _vector b ) {
                _vector v = new _vector();
                    v.x = a.x + b.x;
                    v.y = a.x + b.y;
                    v.z = a.x + b.z;
                return v;
            }
            // <optimized out> - vector sub - 2022-12-15 09:54:04
            public static _vector sub ( _vector a, _vector b ) {
                _vector v = new _vector();
                    v.x = a.x - b.x;
                    v.y = a.x - b.y;
                    v.z = a.x - b.z;
                return v;
            }
            // <optimized out> - vector scale - 2023-01-04 19:16:51
            public static _vector scale ( _vector v, float n ) {
                return new _vector( v.x * n, v.y * n, v.z * n);
            }

            // <optimized out> - vector divide - 2023-01-09 16:35:09
             public static _vector div ( _vector v, float n ) {
                return new _vector( v.x / n, v.y / n, v.z / n);
            }
            // <optimized out> - to vector3 - 2022-12-15 13:46:37
            public static Vector3 to_vector3 ( _vector v ) {
                return new Vector3(v.x, v.y, v.z);
            }
             // <optimized out> - to vector2 - 2022-12-15 14:08:26
            public static Vector2 to_vector2 (_vector v ) {
                return new Vector2(v.x, v.y);
            }
            // <optimized out> - to vector - 2022-12-15 13:48:42
            public static _vector to_vector ( Vector3 v ) {
                return new _vector(v.x, v.y, v.z );
            }
            // <optimized out> - to vector from vector2 - 2022-12-15 14:07:31
            public static _vector to_vector ( Vector2 v ) {
                return new _vector(v.x, v.y, 0.0f);
            }
            // <optimized out>- distance( ) - 2022-12-16 08:39:18
            public static float distance ( _vector a, _vector b ) {
                float x =  (float)Math.Pow((a.x - b.x), 2);
                float y =  (float)Math.Pow((a.y - b.y), 2);
                float z =  (float)Math.Pow((a.z - b.z), 2);
                return     (float)Math.Sqrt(x+y+z);
            }

            // <optimized out> - vector[] to Vector3[] - 2023-01-08 08:36:04
            public static Vector3 [] to_vector3_array ( _vector[] _arr ) {
                if (_arr.Length < 1) { return null; }
                Vector3[] ret = new Vector3[_arr.Length];
                for(int i = 0; i < _arr.Length; i++ ) {
                    ret[i] = to_vector3 ( _arr[i] ); 
                }
                return ret; 
            }

            // <optimized out>.rb - pop( ) - 2022-12-23 20:06:48 
            public static void pop( ref _vector v, float x, float y, float z ) {
                v.x = x; v.y = y; v.z = z; 
            }
  
            // <optimized out> - rotate - 2023-01-23 14:53:59
            public static _vector rotate( char axis, _vector v, float _t) 
            {
                double  t = (double) _t; 
                _vector [] r = new _vector[3]; 
                _vector    o  = new _vector(); 

                /** define */
                switch(axis) {
                  case 'x':
                    /* Rx = ┊ 1   0       0     ┊
                     *      ┊ 0   cos θ  -sin θ ┊
                     *      ┊ 0   sin θ   cos θ ┊
                     */
                    r[0].x = 1.0f;  r[1].x = 0.0f;              r[2].x = 0.0f; 
                    r[1].y = 0.0f;  r[1].y = (float) cos(t); r[2].y = (float) sin (t) * -1.0f; 
                    r[2].z = 0.0f;  r[2].z = (float) sin(t);  r[2].z = (float) cos (t); 
                    break;
                }

                /** multiply */
                o.x = (r[0].x * v.x) + (r[1].x * v.y) + (r[2].x * v.z);
                o.y = (r[0].y * v.x) + (r[1].y * v.y) + (r[2].z * v.z);
                o.z = (r[0].z * v.x) + (r[1].y * v.y) + (r[2].z * v.z); 

                return o; 
            }

        }

        // polar to plane converter 
        public static class plane { 
            
            public static _vector polar_to_cart_coord ( float _long, float _lat, _plane_option opt ) 
            {
                return new _vector( opt.convert()*_long, opt.convert()*_lat, 0.0f); 
            }
        }

        // polar to plane option
        public struct _plane__option {
           public float amplitude;
           public float wave_number; 

           public _plane_poption ( float _amp, float _wn ) {
             this.amplitude = _amp; this.wave_number = _wn;  
           }
            
           public float convert( ) {
             return this.wave_number / luna.cal.trig.wpi;   
           }
        }

        // <optimized out> - clamp - 2023-01-04 20:52:22
        public static class numop {
            public static float clamp ( float val, float min, float max ) {
                if (val < min) return min; else if (val > max) return max; else return val; 
            }
        }

        // ref.six - vector - 2022-12-16 08:54:02 
        public struct _vector { 
            public float x;
            public float y;
            public float z; 
            
            public _vector ( float x, float y, float z ) {
                this.x = x; 
                this.y = y;
                this.z = z; 
            }

            public string to_string( ) {
                string str = "\n";
                str += String.Format("      {0,20}", this.x.ToString("0.000")+"\n");
                str += String.Format("      {0,20}", this.y.ToString("0.000")+"\n");
                str += String.Format("      {0,20}", this.z.ToString("0.000")+"\n"); 
                return str; 
            }
        }
    }
}
