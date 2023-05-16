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
            public static float pi  = (float) Math.PI;
            public static float len = (float) 360.0;   
 
            public delegate double  _cos ( double c );
            public delegate double _acos ( double c );
            public delegate double  _sin ( double c );
            public delegate double _asin ( double c ); 
            public delegate double _atan ( double c ); 

            public static _cos      cos  = new  _cos  ( Math.Cos  );
            public static _acos     acos = new  _acos ( Math.Acos );
            public static _sin       sin = new  _sin  ( Math.Sin  );
            public static _asin     asin = new _asin  ( Math.Asin ); 
            public static _atan    atan  = new _atan  ( Math.Atan ); 

            public static float flip ( float deg )  {
               return (trig.wpi - (deg * trig.d2r)) * trig.r2d;
            }

            public static float quaternion_offset ( float euler_angle  ) 
            {
              float r = euler_angle * d2r; 
              
              if ( r < hpi   ) 
                 return (hpi - r) * r2d; 
              else if ( r >= hpi ) 
                 return ( (450.0f * d2r) - r ) * r2d; 
             
              else return 0.0f; 
            }
        }

        public static class nota { 
            public static float ten3   = (float) Math.Pow(10, 3); 
            public static float nten3 = (float) Math.Pow(10,-3);      
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

            public static _vector[ ] scale ( _vector [] v, float n ) {
                _vector [ ] _v = new _vector[v.Length];
                for(int i = 0; i < _v.Length; i++ ) 
                    _v[i] = scale ( _v[i], n);
                return _v;
            }

            // <optimized out> - vector divide - 2023-01-09 16:35:09
             public static _vector div ( _vector v, float n ) {
                return new _vector( v.x / n, v.y / n, v.z / n);
            }
            // <optimized out> - to vector3 - 2022-12-15 13:46:37
            public static Vector3 to_vector3 ( _vector v ) {
                return new Vector3(v.x, v.y, v.z);
            }

            public static Vector3 to_vector3_yz ( _vector v ) {
               return new Vector3(v.x, v.z, v.y);
            }
             // <optimized out> - to vector2 - 2022-12-15 14:08:26
            public static Vector2 to_vector2 (_vector v ) {
                return new Vector2(v.x, v.y);
            }

            public static Vector2 to_vector2_yz ( _vector v ) {
              return new Vector2(v.x, v.z);
            }
            // <optimized out> - to vector - 2022-12-15 13:48:42
            public static _vector to_vector ( Vector3 v ) {
                return new _vector(v.x, v.y, v.z );
            }

            public static _vector to_vector_yz ( Vector3 v ) {
                return new _vector(v.x, v.z, v.y); 
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

            //convert vector3[ ] to _vector[ ]
            public static _vector[ ] to_vector_array ( Vector3[ ] _arr, bool _yz ) {
                  _vector[ ] r = new _vector[_arr.Length];
                  for(int i = 0; i < _arr.Length; i++ ) 
                  {   
                      if (_yz) 
                        r[ i ] = to_vector_yz ( _arr[ i ] );
                      else 
                        r[ i ] = to_vector      ( _arr[ i ] ); 
                  }
                  return r; 
            }

            // convert _vector[ ] to vector3[ ]
            public static Vector3[ ] to_vector3_array ( _vector[ ] _arr, bool _yz ) {
                if (_arr.Length < 1) { return null; }
                Vector3[] ret = new Vector3[_arr.Length];
                for(int i = 0; i < _arr.Length; i++ ) {
                    if (_yz)
                        ret[i] = to_vector3_yz ( _arr[i] );
                    else 
                        ret[i] = to_vector3      ( _arr[i] ); 
                }
                return ret; 
            }

            public static Vector2[] to_vector2_array ( _vector[] _arr ) {
                Vector2[] r = new Vector2[_arr.Length];
                for(int i = 0; i < _arr.Length; i++ ) { r[i] = to_vector2( _arr[i ] ); } 
                return r; 
            } 

            // <optimized out>.rb - pop( ) - 2022-12-23 20:06:48 
            public static void pop( ref _vector v, float x, float y, float z ) {
                v.x = x; v.y = y; v.z = z; 
            }

            public static _vector[ ] get_radius_vector ( _vector center, float radius, string ori  ) {
              _vector [ ] v = new _vector[(int)trig.len];
              double cur = 0.0;

              for(int i = 0; i < (int) trig.len; i++ ) {
                  switch (ori) {
                    case "horizontal":
                      v[i].x = radius * (float) cos ( cur+= (double) trig.crs) + center.x;
                      v[i].y = radius * (float) sin ( cur+= (double) trig.crs) + center.y;
                      v[i].z = center.z;  
                      break;
                    case "vertical":
                      v[i].x = radius * (float) cos ( cur+= (double) trig.crs) + center.x;
                      v[i].y = center.y;
                      v[i].z = radius * (float) sin ( cur+= (double) trig.crs) + center.z;
                      break;
                    default:
                      luna.debug.log(luna.sysdel.getmethod() + " invalid selection."); 
                      return v; 
                  } 
              }
              return v; 
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
                    r[0].x = 1.0f;  r[1].x = 0.0f;            r[2].x = 0.0f; 
                    r[0].y = 0.0f;  r[1].y = (float) cos(t);  r[2].y = (float) sin (t) * -1.0f; 
                    r[0].z = 0.0f;  r[1].z = (float) sin(t);  r[2].z = (float) cos (t); 
                    break;
                  case 'z': 
                    /* Rz = |  cos θ  sin θ   0 |
                     *      | -sin θ  cos θ   0 |
                     *      |      0      0   1 |
                     */
                     r[0].x = (float) cos(t);         r[1].x = (float) sin(t); r[2].x = 0.0f; 
                     r[0].y = (float) sin(t) * -1.0f; r[1].y = (float) cos(t); r[2].y = 0.0f;
                     r[0].z = 0.0f;                   r[1].z = 0.0f;           r[2].z = 1.0f;
                     break;
                }

                /** multiply */
                o.x = (r[0].x * v.x) + (r[1].x * v.y) + (r[2].x * v.z);
                o.y = (r[0].y * v.x) + (r[1].y * v.y) + (r[2].z * v.z);
                o.z = (r[0].z * v.x) + (r[1].y * v.y) + (r[2].z * v.z); 

                return o; 
            }

            // flip y and z axis of vector3[ ]
            public static Vector3[ ] flip_yz_v3 ( Vector3[ ] _v3 ) {
                Vector3[ ] r3 = new Vector3[_v3.Length];
                for(int i = 0; i < r3.Length; i++ ) {
                    r3[ i ].x = _v3[ i ].x;
                    r3[ i ].y = _v3[ i ].z; 
                    r3[ i ].z = _v3[ i ].y; 
                }
                return r3; 
            }

            // vector magnitude 
            public static float magnitude( _vector v ) 
            {
               return 
               ( 
                  (float) num.sqrt 
                  (
                    num.pow ( v.x, 2.0f ) +
                    num.pow ( v.y, 2.0f ) +
                    num.pow ( v.z, 2.0f )
                  )
               ); 
            }
        }

        // polar to plane converter 
        public static class plane { 
            public static _vector polar_to_cart_coord ( float _long, float _lat, _plane_option _opt ) 
            {
                return new _vector 
                ( 
                    _opt.convert()*  zeroed( trig.pi, _long),
                    _opt.convert()*  (_lat * trig.d2r), 
                    0.0f
                ); 
            }

          public static float zeroed ( float _max_rad,  float _degree ) 
          {
             float rad = _degree * trig.d2r; 
             float pos = _max_rad;
             float neg = _max_rad * -1.0f; 

             if (rad > pos) return neg - (pos - rad);
             if (rad < neg) return pos - (neg - rad);
             return rad; 
          }

          public static float zeroed_unity ( float _max_rad, float _angle ) 
          {
              return _angle / _max_rad; 
          }


          public static _plane_option init_plane_option 
          (
            float amplitude, 
            float wave_number
          ) 
          {
           return new _plane_option ( amplitude, wave_number );  
          }
  
          // obtains right ascension and declination from vector 
          public static (float ra, float dec) calculate_ra_and_dec ( _vector v ) 
          {
              float _ra = 0.0f; float _dec = 0.0f; 
    
              // first, we must obtain the direction of cosines 
              _vector dcos = calculate_direction_of_cosines ( v ); 
            
              // second, we obtain declination (longitude)
              _dec = (float) trig.asin( (double) dcos.z ); 

              // third, we calculate right ascension '_ra'
              float l = dcos.x / (float) trig.cos ( (double) _dec );  
    
              if ( dcos.y > 0.0f ) 
              { 
                _ra = (float) trig.acos ( l ); 
              } 
              else 
              {
                _ra = (float) trig.wpi - ( (float) trig.acos( l ) ); 
              }

              // return right ascension and declination
              return (_ra, _dec); 
          } 
      
          // return the direction of cosines as a vector  
          public static _vector calculate_direction_of_cosines ( _vector v )
          {
            float mag = vector.magnitude ( v ); 
            return new _vector 
            (
              v.x / mag,
              v.y / mag,
              v.z / mag
            );
          }
        }

        // polar to plane option
        public struct _plane_option {
           public float amplitude;
           public float wave_number; 

           public _plane_option ( float _amp, float _wn ) {
             this.amplitude = _amp; this.wave_number = _wn;  
           }
            
           public float convert( ) {
             return this.wave_number / luna.cal.trig.wpi;   
           }

           public string to_string ( )  {
              string str = "plane_option:\n";
              str+= String.Format("amplitude {0,20}", amplitude.ToString()); 
              str+= String.Format("wavenumber {0,20}", wave_number.ToString()); 
              return str; 
           }
        }

        // <optimized out> - clamp - 2023-01-04 20:52:22
        public static class num {

            public static float sqrt ( float v ) 
            {
              return (float) Math.Sqrt ( (double) v ); 
            }

            public static float pow ( float x, float y )
            {
              return (float) Math.Pow( (double) x, (double) y ); 
            }
  
            public static float clamp ( float val, float min, float max ) {
                if (val < min) return min; else if (val > max) return max; else return val; 
            }
            
            public static float negate( float val ) { return val * -1.0f; }

            public static float approach( float current, float dest, float step ) {
                if (dest > current) 
                  return (current + step);
                else if ( dest < current ) 
                  return (current - step); 
                else 
                  return current; 
            }

            public static bool is_modded (int i, int [ ] mods )  
            {
              for(int x= 0; x<mods.Length; x++) {
                if(i%mods[x]==0) return true;
              }
              return false; 
            }

            public static float get_ratio ( float num, float den ) 
            {
              return num / den; 
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

            public void translate( char axis, float n ) {
                switch(axis) {
                  case 'x':
                    this.x += n;
                    break;
                  case 'y':
                    this.y += n;
                    break;
                  case 'z':
                    this.z += n; 
                    break; 
                }
            }

            public void translate( _vector target, float step ) {
               this.x = num.approach ( this.x, target.x, step );
               this.y = num.approach ( this.y, target.y, step );
               this.z = num.approach ( this.z, target.z, step ); 
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
