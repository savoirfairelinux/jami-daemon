#include <cassert>
#include <cstring>
#include <iostream>
#include <cstdlib>   

#ifdef WIN32
#include <time.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#endif

#include "udp.h"
#include "stun.h"

using namespace std;


void
usage()
{
   cerr << "Usage:" << endl
	<< "    ./client stunServerHostname [testNumber] [-v] [-p srcPort] [-i nicAddr1] [-i nicAddr2] [-i nicAddr3]" << endl
	<< "For example, if the STUN server was larry.gloo.net, you could do:" << endl
	<< "    ./client larry.gloo.net" << endl
	<< "The testNumber is just used for special tests." << endl
	<< " test 1 runs test 1 from the RFC. For example:" << endl
	<< "    ./client larry.gloo.net 0" << endl << endl
        << "Return Values:" << endl
        << " -1  Generic Error" << endl << endl
        << "low order bits (mask 0x07)" << endl
        << "  0  No NAT Present (Open)" << endl
        << "  1  Full Cone NAT" << endl
        << "  2  Address Restricted Cone NAT" << endl
        << "  3  Port Restricted Cone NAT" << endl
        << "  4  Symmetric NAT" << endl
        << "  5  Symmetric Firewall" << endl
        << "  6  Blocked or Network Error" << endl 
        << "0x08 bit is set if the NAT does NOT supports hairpinning" << endl 
        << "0x10 bit is set if the NAT tries to preserver port numbers" << endl 
        << endl;
}

#define MAX_NIC 3

int
main(int argc, char* argv[])
{
   assert( sizeof(UInt8 ) == 1 );
   assert( sizeof(UInt16) == 2 );
   assert( sizeof(UInt32) == 4 );
    
   initNetwork();
    
   cout << "STUN client version " << STUN_VERSION << endl;
   
   int testNum = 0;
   bool verbose = false;
	
   StunAddress4 stunServerAddr;
   stunServerAddr.addr=0;

   int srcPort=0;
   StunAddress4 sAddr[MAX_NIC];
   int retval[MAX_NIC];
   int numNic=0;

   for ( int i=0; i<MAX_NIC; i++ )
   {
      sAddr[i].addr=0; 
      sAddr[i].port=0; 
      retval[i]=0;
   }
   
   for ( int arg = 1; arg<argc; arg++ )
   {
      if ( !strcmp( argv[arg] , "-v" ) )
      {
         verbose = true;
      }
      else if ( !strcmp( argv[arg] , "-i" ) )
      {
         arg++;
         if ( argc <= arg ) 
         {
            usage();
            exit(-1);
         }
         if ( numNic >= MAX_NIC )
         {  
            cerr << "Can not have more than "<<  MAX_NIC <<" -i options" << endl;
            usage();
            exit(-1);
         }
         
         stunParseServerName(argv[arg], sAddr[numNic++]);
      }
      else if ( !strcmp( argv[arg] , "-p" ) )
      {
         arg++;
         if ( argc <= arg ) 
         {
            usage();
            exit(-1);
         }
         srcPort = strtol( argv[arg], NULL, 10);
      }
      else    
      {
        char* ptr;
        int t =  strtol( argv[arg], &ptr, 10 );
        if ( *ptr == 0 )
        { 
           // conversion worked
           testNum = t;
           cout << "running test number " << testNum  << endl; 
        }
        else
        {
           bool ret = stunParseServerName( argv[arg], stunServerAddr);
           if ( ret != true )
           {
              cerr << argv[arg] << " is not a valid host name " << endl;
              usage();
              exit(-1);
           }
	}	
      }
   }
   
   if ( numNic == 0 )
   {
      // use default 
      numNic = 1;
   }
   
   for ( int nic=0; nic<numNic; nic++ )
   {
      sAddr[nic].port=srcPort;
      if ( stunServerAddr.addr == 0 )
      {
         usage();
         exit(-1);
      }
   
      if (testNum==0)
      {
         bool presPort=false;
         bool hairpin=false;
		
         NatType stype = stunNatType( stunServerAddr, verbose, &presPort, &hairpin, 
                                      srcPort, &sAddr[nic]);
		
         if ( nic == 0 )
         {
            cout << "Primary: ";
         }
         else
         {
            cout << "Secondary: ";
         }
         
         switch (stype)
         {
            case StunTypeFailure:
               cout << "Some error detetecting NAT type";
	       retval[nic] = -1;
               exit(-1);
               break;
            case StunTypeUnknown:
               cout << "Some error detetecting NAT type";
	       retval[nic] = 0x07;
               break;
            case StunTypeOpen:
               cout << "Open";
	       retval[nic] = 0; 
               break;
            case StunTypeConeNat:
               cout << "Full Cone Nat";
               if ( presPort ) cout << ", preserves ports"; else cout << ", random port";
               if ( hairpin  ) cout << ", will hairpin"; else cout << ", no hairpin";
               retval[nic] = 0x01;
               break;
            case StunTypeRestrictedNat:
               cout << "Address Restricted Nat";
               if ( presPort ) cout << ", preserves ports"; else cout << ", random port";
               if ( hairpin  ) cout << ", will hairpin"; else cout << ", no hairpin";
               retval[nic] = 0x02;
               break;
            case StunTypePortRestrictedNat:
               cout << "Port Restricted Nat";
               if ( presPort ) cout << ", preserves ports"; else cout << ", random port";
               if ( hairpin  ) cout << ", will hairpin"; else cout << ", no hairpin";
               retval[nic] = 0x03;
               break;
            case StunTypeSymNat:
               cout << "Symmetric Nat";
               if ( presPort ) cout << ", preserves ports"; else cout << ", random port";
               if ( hairpin  ) cout << ", will hairpin"; else cout << ", no hairpin";
               retval[nic] = 0x04;
               break;
            case StunTypeSymFirewall:
               cout << "Symmetric Firewall";
               if ( hairpin  ) cout << ", will hairpin"; else cout << ", no hairpin";
               retval[nic] = 0x05;
               break;
            case StunTypeBlocked:
               cout << "Blocked or could not reach STUN server";
               retval[nic] = 0x06;
               break;
            default:
               cout << stype;
               cout << "Unkown NAT type";
               retval[nic] = 0x07;  // Unknown NAT type
               break;
         }
         cout << "\t"; cout.flush();
         
         if (!hairpin)
         {
             retval[nic] |= 0x08;
         }       

         if (presPort)
         {
             retval[nic] |= 0x10;
         }
      }
      else if (testNum==100)
      {
         Socket myFd = openPort(srcPort,sAddr[nic].addr,verbose);
      
         StunMessage req;
         memset(&req, 0, sizeof(StunMessage));
      
         StunAtrString username;
         StunAtrString password;
         username.sizeValue = 0;
         password.sizeValue = 0;
      
         stunBuildReqSimple( &req, username, 
                             false , false , 
                             0x0c );
      
         char buf[STUN_MAX_MESSAGE_SIZE];
         int len = STUN_MAX_MESSAGE_SIZE;
      
         len = stunEncodeMessage( req, buf, len, password,verbose );
      
         if ( verbose )
         {
            cout << "About to send msg of len " << len 
                 << " to " << stunServerAddr << endl;
         }
      
         while (1)
         {
            for ( int i=0; i<100; i++ )
            {
               sendMessage( myFd,
                            buf, len, 
                            stunServerAddr.addr, 
                            stunServerAddr.port,verbose );
            }
#ifdef WIN32 // !cj! TODO - should fix this up in windows
            clock_t now = clock();
            assert( CLOCKS_PER_SEC == 1000 );
            while ( clock() <= now+10 ) { };
#else
            usleep(10*1000);
#endif
         }
      }
      else if (testNum==-2)
      {
         const int numPort = 5;
         int fd[numPort];
         StunAddress4 mappedAddr;
         
         for( int i=0; i<numPort; i++ )
         {
            fd[i] = stunOpenSocket( stunServerAddr, &mappedAddr,
                                    (srcPort==0)?0:(srcPort+i), &sAddr[nic],
                                    verbose );
            cout << "Got port at " << mappedAddr.port << endl;
         }
          
         for( int i=0; i<numPort; i++ )
         {
            closesocket(fd[i]);
         }
      }
      else if (testNum==-1)
      {
         int fd3,fd4;
         StunAddress4 mappedAddr;
         
         bool ok = stunOpenSocketPair(stunServerAddr,
                                      &mappedAddr,
                                      &fd3,
                                      &fd4,
                                      srcPort, 
                                      &sAddr[nic],
                                      verbose);
         if ( ok )
         {
            closesocket(fd3);
            closesocket(fd4);
            cout << "Got port pair at " << mappedAddr.port << endl;
         }
         else
         {
            cerr << "Opened a stun socket pair FAILED" << endl;
         }
      }
      else
      {
         stunTest( stunServerAddr,testNum,verbose,&(sAddr[nic]) );
      }
   } // end of for loop 
   cout << endl;
   
   UInt32 ret=0;
   for ( int i=numNic-1; i>=0; i-- )
   {
      if ( retval[i] == -1 )
      {
         ret = 0xFFFFFFFF;
         break;
      }
      ret = ret << 8;
      ret = ret | ( retval[i] & 0xFF );
   }
   
   cout << "Return value is " << hex << "0x" << ret << dec << endl;
   
   return ret;
}


/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */

// Local Variables:
// mode:c++
// c-file-style:"ellemtel"
// c-file-offsets:((case-label . +))
// indent-tabs-mode:nil
// End:
