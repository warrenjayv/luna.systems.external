using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Diagnostics;
using System.Text;

using luna.external.netrucks;
using luna.external.console;

namespace luna {
   namespace external {
       namespace console {

            public static class _EXTERN_FLAGS 
            {
                public static bool _DO_LOG                    = false; 
                public static bool _OUTWRITER_ACTIVE  = false; 

                public static int  _LOG_SPEED                = 500; 
            }

            public class prog  {
              public static _net_param spm = new _net_param();

              static void Main(string[] args) 
              { 
                helper(); 
            
              }

              public static void start_server () {
                  Thread thread = new Thread(server); thread.Start(); 
              }
              public static void server () {

                  // initialize end point 
                  IPHostEntry host = Dns.GetHostEntry(Dns.GetHostName()); 

                  // lists all ip and determines selection
                  (bool, int) stat    = list_ip_addresses ( host );

                  if (! stat.Item1 ) { 
                      log("ERROR: no network devices found!", color.red); 
                      return; 
                  }

                  // request server port //  request_port();

                  IPAddress    ipaddr  = host.AddressList[stat.Item2];
                  IPEndPoint  iep       = new IPEndPoint(ipaddr, spm.server_port);
                  Socket recvr = new Socket(ipaddr.AddressFamily, SocketType.Stream, ProtocolType.Tcp);

                  try {
                      recvr.Bind(iep) ;  
                      recvr.Listen(spm.max_connections);

                          EndPoint? local = (EndPoint)recvr.LocalEndPoint;
                          write("  server ip address: ", color.blue,  IPAddress.Parse(((IPEndPoint)local).Address.ToString()).ToString(), color.green); 
                          write("  server port: ", color.blue, ((IPEndPoint)local).Port.ToString(), color.green);
                          write("  receiver initiated.",  color.green);

                      Socket? client = recvr.Accept(); 
                      // server loop 
                      while(true) {
                            byte[]  bytes = new Byte[spm.buffer_length];
                            string? data   = null;
                            int _bytes     = 0;

                            while(true) {
                                 _bytes = client.Receive(bytes);
                                 data += Encoding.ASCII.GetString(bytes, 0, _bytes);

                                 //if(data.IndexOf("<EOF>") > -1) break; 
                                if (_bytes > 0) { break; }
                            }

                            if(data != null) { 
                                log("data received; size: " + _bytes.ToString() + " bytes", color.blue);
                                write( data, color.mag);
                                //split_write(data, color.mag);
                            }
                      }

                      client.Shutdown(SocketShutdown.Both);
                      client.Close();
                   }
                  catch (Exception e) {
                    log(e.ToString(), color.red);
                  }
              } // server ( ) { }

              // helper statements
              public static string[] command_names = { "help", "run", "set port", "set ip", "do log", "diagnose", "quit" };
              public static string[] command_defs  = 
                { 
                  "list all available commands",
                  "runs the server",
                  "sets the server port",
                  "set the server ip",
                  "write to file 'out.log'",
                  "developer check",
                  "closes the application"
                };

              public static void list_commands (  ) {
                  for (int i = 0; i < command_names.Length; i++ ) {
                      write( String.Format("{0,8} {1} {2,-1}", command_names[i], " - ", command_defs[i]), color.blue);
                  }
              }

              public static void do_log ( ) 
              {    
                  _EXTERN_FLAGS._DO_LOG = ! _EXTERN_FLAGS._DO_LOG; 

                  if (! _EXTERN_FLAGS._OUTWRITER_ACTIVE) 
                  {
                     Thread thread = new Thread(outwriter.daemon); thread.Start(); 
                      if (thread.IsAlive) 
                      {
                         write("outwriter daemon started.", color.mag);
                         _EXTERN_FLAGS._OUTWRITER_ACTIVE = true; 
                      }
                  }
              }

              public static void diagnosis( ) 
              {
                  string diag = "\n";
                  diag += "threads: " + Process.GetCurrentProcess( ).Threads.Count.ToString( ) + "\n";
                  diag += "flags\n";
                  diag +="logs: "                          +  _EXTERN_FLAGS._DO_LOG.ToString( )                    + '\n';
                  diag +="writer thread active?: " + _EXTERN_FLAGS._OUTWRITER_ACTIVE.ToString( )   + '\n';
                  write(diag, color.mag);
              }

              public static void helper ( ) 
              {  
                    string? msg = "";
                    write(DateTime.Now.ToString(), color.darkmag);
                    write("luna.external.console", color.white, ".debugger", color.mag);
                    write("for information, type ", color.gray, "help", color.green);
                    while(true) { 
                        color.set(color.gray); msg = Console.ReadLine();
                        helper_match(msg);
                    }
              } // helper ( )  { }

              public static void helper_match ( string? msg ) {
                  switch (msg) {
                    case "": break;
                    case "help": 
                        list_commands();
                        break;
                    case "run":
                        start_server();
                        break;
                    case "set port":
                        request_port();
                        break;
                    case "set ip":
                        select_ip_address();
                        break;
                    case "do log":
                        do_log();
                        break;
                    case "diagnose":
                        diagnosis();
                        break;
                    case "quit":
                       Environment.Exit(0);
                       break;   
                    default:
                       write("invalid selection. see help", color.red);
                       break;
                  }
              }

              public static void log(string msg) {
                string decore =  typeof(prog) + " " + DateTime.Now.ToString( ) + ": " + msg; 
                Console.WriteLine(decore); 
                outwriter.update(decore);
              }

              public static void log(string msg, ConsoleColor c) {
                color.set(c);
                string decore = typeof(prog) + " " + DateTime.Now.ToString( ) + ": " + msg; 
                Console.WriteLine(decore);
                 outwriter.update(decore);
                color.set(color.white);
              }

              public static void write(string msg, ConsoleColor c) {
                color.set(c);
                Console.WriteLine(msg); 
                outwriter.update(msg); 
                color.set(color.white);
              }

              public static void write(string word1, ConsoleColor c1, string word2, ConsoleColor c2) {
                color.set(c1); Console.Write(word1);
                color.set(c2); Console.Write(word2);
                color.set(color.white); Console.WriteLine("");
              }

              public static void write_out(string msg) 
              {
                  if (_EXTERN_FLAGS._DO_LOG )
                  {
                      using (var sw = new StreamWriter("out.log", true))
                      {
                        sw.Write(msg);
                      }
                  }
              }

              public static void split_write (string msg, ConsoleColor c) {
                  string[] arr = msg.Split("\n"); 
                  var t = (0, 0); t = group(arr);
                  color.set(c);
                  for(int i = 0; i < arr.Length - 1; i+=2) {
                          if(i == t.Item1 || (i+1) == t.Item1) {
                              for (int j = t.Item1; j < (t.Item1+t.Item2); j++) {
                                Console.WriteLine(String.Format("{0,12}", arr[j])); 
                                i++; 
                              }
                          }
                          if ((i+1) >= arr.Length) {
                              Console.WriteLine(String.Format("{0,12}", arr[i])); 
                              return; 
                          }
                          Console.WriteLine(String.Format("{0,12} {1,30}", arr[i], arr[i+1]));
                      }
                  color.set(color.white);
              }

              internal static (int f, int o) group ( string [] arr ) {
                int first = 0, offset = 0; 
                for (int i = 0; i < arr.Length; i++ ) {
                  if (arr[i].StartsWith('{') && arr[i].EndsWith('}')) {
                    first = i+1; offset = (int)Char.GetNumericValue(arr[i][1]); 
                    arr[i] = " ";
                  }
                }
                return(first, offset);
              } 

              public static void request_port ( ) {
                    log("please enter desired port for this server: ", color.mag);
                    spm.server_port = Convert.ToInt32(Console.ReadLine()); 
                    write("port set to ", color.blue, spm.server_port.ToString(), color.mag);
              }

              public static (bool, int) list_ip_addresses ( IPHostEntry host ) {

                spm.is_ip_set = false; 

                log("server thread initiated: ", color.green);
                write("use command: ", color.blue, "set ip", color.mag);

                if (host.AddressList.Length == 0) { 
                  log("no network devices found.", color.red);
                  return (false, 0);
                }

                while (! spm.is_ip_set) { if (spm.is_ip_set) break; }
              
                return (true, spm.selection);
              }

              public static void select_ip_address ( ) {
                IPHostEntry host = Dns.GetHostEntry(Dns.GetHostName());                  

                write("list of available addresses. ", color.blue);

                for(int i =0 ; i < host.AddressList.Length; i++ ) 
                {
                  write("  ["+i+"] "+host.AddressList[i], color.blue);
                }
               
                while (true) {
                  try {
                    write("please select an ip address index ", color.mag);
                    spm.selection = Convert.ToInt32(Console.ReadLine()); 
                     if (spm.selection >= host.AddressList.Length || spm.selection < 0 ) throw new InvalidSelectionException(); else break; 
                  } catch (InvalidSelectionException e) {
                    System.Diagnostics.Trace.WriteLine("InvalidSelectionException");
                  } catch (Exception e) {
                    write(e.ToString(), color.red);
                  }
                }

                write("you have selected [" + spm.selection.ToString() + "]", color.white, host.AddressList[spm.selection].ToString(), color.mag);
                spm.is_ip_set = true; 
              }

            }// prog { } 

           public static class color {
                public static ConsoleColor red      = ConsoleColor.Red;
                public static ConsoleColor blue    = ConsoleColor.Blue; 
                public static ConsoleColor yellow = ConsoleColor.Yellow;
                public static ConsoleColor white   = ConsoleColor.White;
                public static ConsoleColor green   = ConsoleColor.Green;
                public static ConsoleColor mag      = ConsoleColor.Magenta;
                public static ConsoleColor cyan     = ConsoleColor.Cyan; 
                public static ConsoleColor gray     = ConsoleColor.Gray;
                public static ConsoleColor darkmag = ConsoleColor.DarkMagenta;
                

                public static void set( ConsoleColor c ) {
                  Console.ForegroundColor = c; 
                }
             }

            public class InvalidSelectionException : Exception
            {
              public InvalidSelectionException(  )  {
                  log("selection out of range. try again.", color.red );
              }

              public static void log(string msg, ConsoleColor c) {
                   color.set(c);
                   Console.WriteLine(typeof(InvalidSelectionException) + " " + DateTime.Now.ToString() + ": " + msg);
                   color.set(color.white);
              } 

            }//InvalidSelectionException { } 

          }//console{} 

          namespace netrucks {
                public struct _net_param {
                  public int buffer_length;
                  public int server_port;
                   public int max_connections;
                   public int selection;
                   public bool is_ip_set;
                   public bool is_port_set; 

                   public _net_param( ) {
                      this.buffer_length      =  8042;
                      this.server_port          = 51111;
                      this.max_connections = 24;
                      this.selection             = 0;
                      this.is_ip_set             = false;
                      this.is_port_set          = false;
                   }
              }
          }

          public static class assert 
          {
              public static bool is_null<T>(T arg) { if (arg == null ) return true; else return false; }
          }
   }
}
