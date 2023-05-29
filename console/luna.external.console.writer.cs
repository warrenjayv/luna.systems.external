using System;
using System.Threading;
using System.IO;

using luna.external.console;

namespace luna 
{
    namespace external 
    {
      namespace console
      {
        public static class outwriter 
        {
            public static string logfile = "out.log";
            public static string log = "outwriter logger active.";

            public static void daemon ( )
            {

              while(true)
              {
                  write_to_log( ); 
                  //Thread.Sleep(_EXTERN_FLAGS._LOG_SPEED);
              }
                
            } 
             
            public static void write_to_log( )
            {
                if (_EXTERN_FLAGS._DO_LOG) 
                { 
      
                    // avoid  duplicates
                    if (File.Exists(logfile)) 
                    {
                      string cs = File.ReadAllText(logfile); 
                      if(cs.Contains(log)) { return; }
                    }

                    // append to log file 
                    using (var sw = File.AppendText(logfile))
                    {
                        sw.Write(log + "\n");
                        sw.Close();
                        
                    }

                    log = " ";

                }
            }

            public static void update ( string msg ) 
            {
              log = msg; 
            }
        }
      }
    }
}