using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Collections;
using System.Collections.Generic;


namespace luna
{
    namespace external {
        namespace debug {
            public static class client { 

                public static _net_param npm = new _net_param();
                public static void execute_client ( ) {
                    try {
                        IPHostEntry host    = Dns.GetHostEntry( Dns.GetHostName());
                        IPAddress  ipaddr   = host.AddressList[0];
                        IPEndPoint remote = new IPEndPoint(ipaddr, npm.server_port);             

                        Socket sendr = new Socket(ipaddr.AddressFamily, SocketType.Stream, ProtocolType.Tcp); 

                        try
                        {
                            sendr.Connect(remote);
                            log("connection attempt: " + sendr.RemoteEndPoint.ToString());
                            byte[] msg = message_in_bytes("suck my cock<EOF>");
                            int bytes    = sendr.Send(msg);
                            
                        }
                        catch (Exception e) 
                        {
                            luna.debug.log(typeof(client) + ": " + e.ToString());
                        }   

                    } 
                    catch (Exception e ) {
                            luna.debug.log(typeof(client) + ": " + e.ToString());
                    }
                }

                public static byte[] message_in_bytes ( string msg ) {
                    return Encoding.ASCII.GetBytes(msg);
                }

                public static void log ( string msg ) {
                    luna.debug.log(typeof(client) + ": " + msg);
                }
            }

            public struct _net_param {
                public int buffer_length;
                public int server_port;
                public int max_connections;

                public _net_param ( int nullify ) {
                      this.buffer_length     = 1024;
                      this.server_port         = 51111;
                      this.max_connections = 24;
                }
            }
        }
    }
}
