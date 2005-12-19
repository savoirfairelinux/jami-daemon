#include <cassert>
#include <cstring>
#include <iostream>
#include <cstdlib>   

#ifndef WIN32
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#endif

#include "udp.h"
#include "stun.h"


using namespace std;


void 
usage()
{
   cerr << "Usage: " << endl
        << " ./server [-v] [-h] [-h IP_Address] [-a IP_Address] [-p port] [-o port] [-m mediaport]" << endl
        << " " << endl
        << " If the IP addresses of your NIC are 10.0.1.150 and 10.0.1.151, run this program with" << endl
        << "    ./server -v  -h 10.0.1.150 -a 10.0.1.151" << endl
        << " STUN servers need two IP addresses and two ports, these can be specified with:" << endl
        << "  -h sets the primary IP" << endl
        << "  -a sets the secondary IP" << endl
        << "  -p sets the primary port and defaults to 3478" << endl
        << "  -o sets the secondary port and defaults to 3479" << endl
        << "  -b makes the program run in the backgroud" << endl
        << "  -m sets up a STERN server starting at port m" << endl
        << "  -v runs in verbose mode" << endl
      // in makefile too
        << endl;
}


int
main(int argc, char* argv[])
{
   assert( sizeof(UInt8 ) == 1 );
   assert( sizeof(UInt16) == 2 );
   assert( sizeof(UInt32) == 4 );

   initNetwork();

   clog << "STUN server version "  <<  STUN_VERSION << endl;
      
   StunAddress4 myAddr;
   StunAddress4 altAddr;
   bool verbose=false;
   bool background=false;
   
   myAddr.addr = 0;
   altAddr.addr = 0;
   myAddr.port = STUN_PORT;
   altAddr.port = STUN_PORT+1;
   int myPort = 0;
   int altPort = 0;
   int myMediaPort = 0;
   
   UInt32 interfaces[10];
   int numInterfaces = stunFindLocalInterfaces(interfaces,10);

   if (numInterfaces == 2)
   {
      myAddr.addr = interfaces[0];
      myAddr.port = STUN_PORT;
      altAddr.addr = interfaces[1];
      altAddr.port = STUN_PORT+1;
   }

   for ( int arg = 1; arg<argc; arg++ )
   {
      if ( !strcmp( argv[arg] , "-v" ) )
      {
         verbose = true;
      }
      else if ( !strcmp( argv[arg] , "-b" ) )
      {
         background = true;
      }
      else if ( !strcmp( argv[arg] , "-h" ) )
      {
         arg++;
         if ( argc <= arg ) 
         {
            usage();
            exit(-1);
         }
         stunParseServerName(argv[arg], myAddr);
      }
      else if ( !strcmp( argv[arg] , "-a" ) )
      {
         arg++;
         if ( argc <= arg ) 
         {
            usage();
            exit(-1);
         }
         stunParseServerName(argv[arg], altAddr);
      }
      else if ( !strcmp( argv[arg] , "-p" ) )
      {
         arg++;
         if ( argc <= arg ) 
         {
            usage();
            exit(-1);
         }
         myPort = UInt16(strtol( argv[arg], NULL, 10));
      }
      else if ( !strcmp( argv[arg] , "-o" ) )
      {
         arg++;
         if ( argc <= arg ) 
         {
            usage();
            exit(-1);
         }
         altPort = UInt16(strtol( argv[arg], NULL, 10));
      }
      else if ( !strcmp( argv[arg] , "-m" ) )
      {
         arg++;
         if ( argc <= arg ) 
         {
            usage();
            exit(-1);
         }
         myMediaPort = UInt16(strtol( argv[arg], NULL, 10));
      }
      else
      {
         usage();
         exit(-1);
      }
   }

   if ( myPort != 0 )
   {
      myAddr.port = myPort;
   }
   if ( altPort != 0 )
   {
      altAddr.port = altPort;
   }
   
   if (  (myAddr.addr == 0) || (altAddr.addr == 0) )
   {
      clog << "If your machine does not have exactly two ethernet interfaces, "
           << "you must specify the server and alt server" << endl;
      //usage();
      //exit(-1);
   }

   if ( myAddr.port == altAddr.port )
   {
      altAddr.port = myAddr.port+1;
   }

   if ( verbose )
   {
      clog << "Running with on interface "
           << myAddr << " with alternate " 
           << altAddr << endl;
   }
    
   if (
      ( myAddr.addr  == 0 ) ||
      ( myAddr.port  == 0 ) ||
      //( altAddr.addr == 0 ) ||
      ( altAddr.port == 0 ) 
      )
   {
      cerr << "Bad command line" << endl;
      exit(1);
   }
   
   if ( altAddr.addr == 0 )
   {
      cerr << "Warning - no alternate ip address STUN will not work" << endl;
      //exit(1);
   }
   
#if defined(WIN32)
   int pid=0;

   if ( background )
   {
      cerr << "The -b background option does not work in windows" << endl;
      exit(-1);
   }
#else
   pid_t pid=0;

   if ( background )
   {
      pid = fork();

      if (pid < 0)
      {
         cerr << "fork: unable to fork" << endl;
         exit(-1);
      }
   }
#endif

   if (pid == 0) //child or not using background
   {
      StunServerInfo info;
      bool ok = stunInitServer(info, myAddr, altAddr, myMediaPort, verbose);
      
      int c=0;
      while (ok)
      {
         ok = stunServerProcess(info, verbose);
         c++;
         if ( c%1000 == 0 ) 
         {
            clog << "*";
         }
      }
      // Notreached
   }
   
   return 0;
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
