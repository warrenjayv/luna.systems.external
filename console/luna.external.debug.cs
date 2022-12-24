using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;


namespace luna
{
    namespace external {
        namespace debug {
            public static class client { 

                public static _net_param npm = new _net_param(1024, 51111, 24, 3);
                public static void execute_client ( string _msg  ) {
                    try {
                        IPHostEntry host    = Dns.GetHostEntry(Dns.GetHostName());
                        IPAddress  ipaddr   = host.AddressList[npm.ip_index];
                        IPEndPoint remote = new IPEndPoint(ipaddr, npm.server_port);             

                        if (remote == null ) { log ("IPEndPoint null."); return; }
                        Socket sendr = new Socket(ipaddr.AddressFamily, SocketType.Stream, ProtocolType.Tcp); 

                        try
                        {
                            sendr.Connect(ipaddr, npm.server_port);
                            log("connection attempt: " + sendr.RemoteEndPoint.ToString());
                            byte[] msg = message_in_bytes(_msg);
                            int bytes    = sendr.Send(msg);
                            
                        }
                        catch (Exception e) 
                        {
                          log(e.ToString());
                        }   

                    } 
                    catch (Exception e ) 
                    {
                        log(e.ToString());
                    }
                }

                public static byte[] message_in_bytes ( string msg ) {
                    return Encoding.ASCII.GetBytes(msg);
                }

                public static void log ( string msg ) {
                   Debug.Log(typeof(client) + ": " + msg);
                }

                public static void list_ip_addresses (  ) {
                    IPHostEntry host    = Dns.GetHostEntry(Dns.GetHostName());
                    log("ip address list [ ]: ");
                    for(int i =0; i < host.AddressList.Length; i++ ) {
                        Debug.Log(" " + host.AddressList[i].ToString());
                    }
                }

                public static void select_ip_index ( ) {
                    IPHostEntry host    = Dns.GetHostEntry(Dns.GetHostName());
                    npm.ip_index       += 1;
                    if (npm.ip_index > host.AddressList.Length) {
                        npm.ip_index = 0; 
                    }
                    log("ip index set: " + "[" + npm.ip_index + "] " + host.AddressList[npm.ip_index].ToString()); 
                }

            }

            public struct _net_param {
                public int buffer_length;
                public int server_port;
                public int max_connections;
                public int ip_index; 

                public _net_param ( int len, int port, int max, int i  ) {
                      this.buffer_length     = len;
                      this.server_port         = port;
                      this.max_connections = max;
                      this.ip_index              = i;
                }
            }
        }
    }
}
