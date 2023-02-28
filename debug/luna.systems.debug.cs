using System;

namespace luna {
    namespace systems {
        namespace debug {
            public static class debug {
                public static bool is_null<T>(T arg ) { if (arg == null) return true; else return false;}
                public static bool is_null<T>(T arg, int _n_ ) { 
                    try {
                      string n = String.Format("#{0:X}", arg);  return false; 
                    } 
                    catch {
                      return true; 
                    }
                }
            }
        }
    }
}