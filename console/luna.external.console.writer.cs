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
                  Thread.Sleep(_EXTERN_FLAGS._LOG_SPEED);
              }
                
            } 
             
            public static void write_to_log( )
            {
                if (_EXTERN_FLAGS._DO_LOG) 
                { 
                    FileStream stream = new FileStream
                    (
                        logfile,
                        FileMode.OpenOrCreate,
                        FileAccess.ReadWrite, 
                        FileShare.None
                    );

                    // check for duplicate
                    using (var sr = new StreamReader( stream )) 
                    {
                        string rdr = sr.ReadToEnd( );
                        if(rdr.Contains(log)) 
                        {
                            sr.Close( );
                            return;
                        }

                    }
                   
                    // append to log file 
                    using (var sw = File.AppendText(logfile))
                    {
                        sw.Write(log + "\n");
                        sw.Close();
                    }
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