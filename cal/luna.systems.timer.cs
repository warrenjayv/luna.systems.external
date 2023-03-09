using System;
using System.Threading;

namespace luna {
  namespace systems {
    namespace timer {
      public class _time {
        Thread   tr; 
        internal float duration;
        internal bool  complete; 

        public _time( ) {
          complete = false; 
        }

        public void wait( float _dur ) {
          duration = _dur; 
          tr       = new Thread(wait_thread); 
          tr.Start( ); 
        }

        internal void wait_thread ( ) {
          Thread.Sleep((int)duration);
          complete = true; 
        }

        public void reset( ) { complete = false; }

        public bool get_wait( ) {
           return complete; 
        }
      }
    } 
  }
}
