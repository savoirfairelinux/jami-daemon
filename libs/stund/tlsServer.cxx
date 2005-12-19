
/* 
   This program takes no arguments. It opens the STUN tcp port and rus a tls server
   on it. 
*/

#include <sys/types.h>
#include <string.h>
#include <openssl/e_os2.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#ifdef WIN32
#include <errno.h>
#include <winsock2.h>
#include <io.h>
#endif
 
#ifndef WIN32
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#ifdef WIN32
# include <windows.h>
# include <winbase.h>
# include <errno.h>
# include <winsock2.h>
# include <io.h>
typedef unsigned int u_int32_t;
#endif

#include <cassert>
#include <fcntl.h>

#include <iostream>
#include <cstdlib>
#include <stdio.h>

#ifdef __MACH__
typedef int socklen_t;
#endif

#include "stun.h"

using namespace std;



#define MAX_CONNECTIONS 64


#ifdef WIN32
typedef int socklen_t;
//#define errno WSAGetLastError()
typedef SOCKET Socket;
#else
typedef int Socket;
static const Socket INVALID_SOCKET = -1;
static const int SOCKET_ERROR = -1;
#endif



// TODO - !cj! - need to deal with closing connections 


void 
makeSocketNonBlocking(Socket fd)
{
#if WIN32
   unsigned long noBlock = 1;
   int errNoBlock = ioctlsocket( fd, FIONBIO , &noBlock );
   if ( errNoBlock != 0 )
   {
      assert(0);
   }
#else
   int flags  = fcntl( fd, F_GETFL, 0);
   int errNoBlock = fcntl(fd, F_SETFL, flags | O_NONBLOCK );
   if ( errNoBlock != 0 ) // !cj! I may have messed up this line 
   {
      assert(0);
   }
#endif
}


int 
main()
{
#ifdef WIN32 
   WORD wVersionRequested = MAKEWORD( 2, 2 );
   WSADATA wsaData;
   int err;

   err = WSAStartup( wVersionRequested, &wsaData );
   if ( err != 0 ) 
   {
      // could not find a usable WinSock DLL
      //cerr << "Could not load winsock" << endl;
      assert(0); // is this is failing, try a different version that 2.2, 1.0 or later will likely work 
      exit(1);
   }
#endif

   // contexts for each connection 
   SSL* ssl[MAX_CONNECTIONS];

   // buffers for each connection
   BIO* bio[MAX_CONNECTIONS];

   // file descriptors for each connection
   Socket fd[MAX_CONNECTIONS];

   // ip address of other side of connection 
   int  peerip[MAX_CONNECTIONS];

   // root cert list 
   X509_STORE* certAuthorities;
   // my public cert
   X509* publicCert;
   // my private key 
   EVP_PKEY* privateKey;
   // SSL Context 
   SSL_CTX* ctx;
   
   Socket mFd;
   
   char* password = "password";
   

   for ( int i=0; i<MAX_CONNECTIONS; i++ )
   {
      fd[i]=-1;
      bio[i]=0;
      ssl[i]=0;
   }
   

   // load public cert 
   FILE* fp = fopen("id.pem","rb");
   if ( !fp )
   {
      cerr << "Could not read public cert" <<endl; exit(1);
   }
   publicCert = PEM_read_X509(fp,NULL,NULL,NULL);
   if (!publicCert)
   {
      cerr << "Error reading contents of public cert"<<endl; exit(1);
   }
   cerr << "Loaded public cert"<<endl;


   // load private key 
   fp = fopen("id_key.pem","rb");
   if ( !fp )
   {
      cerr << "Could not read private key"<<endl; exit(1);
   }
   privateKey = PEM_read_PrivateKey(fp,NULL,NULL,password);
   if (!privateKey)
   {
      cerr << "Error reading contents of private key file "<<endl;exit(1);
   }
   cerr << "Loaded private key "<<endl;
   
   // load root certs 
   certAuthorities = X509_STORE_new();
   assert( certAuthorities );
   if ( X509_STORE_load_locations(certAuthorities,"root.pem",NULL) != 1 )
   {  
      cerr << "Error reading contents of root cert file "<<endl;
   }
   cerr << "Loaded public CAs"<<endl;

   
   // set up main security context    
   SSL_library_init();
   
   SSL_load_error_strings();
   
   OpenSSL_add_all_ciphers();
   OpenSSL_add_all_digests();
   
   ERR_load_crypto_strings();
   
   ctx=SSL_CTX_new( TLSv1_method() );
   assert( ctx );
   
   int ok;
   ok = SSL_CTX_use_certificate(ctx, publicCert);
   assert( ok == 1);
   
   ok = SSL_CTX_use_PrivateKey(ctx,privateKey);
   assert( ok == 1);
   
   assert( certAuthorities );
   SSL_CTX_set_cert_store(ctx, certAuthorities);
   
   
   // open a socket to listen for requests on 
   mFd = socket(PF_INET, SOCK_STREAM, 0);

   sockaddr_in addr;
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY); 
   addr.sin_port = htons(STUN_PORT);
   
   if ( bind( mFd, (struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR )
   {
      int err = errno;
      if ( err == EADDRINUSE )
      {
         cerr << "Port already in use"<<endl;
      }
      else
      {
         cerr << "Could not bind to port"<<endl;
      }
      
      exit(0);
   }

   makeSocketNonBlocking(mFd);

   int e = listen(mFd,64 );
   if (e != 0 )
   {
      assert(0);
   }


   cerr << "Ready for requests" << endl;
   
   while ( true ) 
   {
      // set up the read fd set for select 
      fd_set read;
      FD_ZERO(&read);
      int size=0;

      FD_SET(mFd, &read);
      size = mFd+1;

      for ( int i=0; i<MAX_CONNECTIONS; i++ )
      {
         if ( fd[i] > 0 )
         {
            FD_SET(fd[i], &read); 
            size = ( int(fd[i]+1) > size) ? int(fd[i]+1) : size;
         }
         
      }


      // do a select 
      unsigned long ms = 500;
      struct timeval tv;
      tv.tv_sec = (ms/1000);
      tv.tv_usec = (ms%1000)*1000;
      int e = select(size, &read, NULL, NULL, &tv);
      
      cerr << "." ;
      
      // process any new connections 
      if ( FD_ISSET(mFd, &read))
      {
         cerr << "Got a new connection" << endl;
         
         // find an unused connection 
         int i=0;
         for ( ; i<MAX_CONNECTIONS; i++ )
         {
            if ( fd[i] == -1 ) 
            {
               break;
            }
         }
         
         if ( i >= MAX_CONNECTIONS )
         {
            cerr << "Ran out of connections to use "<<endl;
            break;
         }
         
         struct sockaddr_in peer;
         int peerLen=sizeof(peer);
         fd[i] = accept( mFd, (struct sockaddr*)&peer,(socklen_t*)&peerLen);
         if ( fd[i] == -1 )
         {
            int err = errno;
            cerr << "Error on accept: " << strerror(err)<<endl;
            break;
         }
         
         int* ptr =  (int*)( &peer.sin_addr );
         peerip[i] = *ptr;
         
         ssl[i] = NULL;
         
         ssl[i] = SSL_new(ctx);
         assert(ssl[i]);
         
         bio[i] = BIO_new_socket(fd[i],0/*close flag*/);
         assert( bio[i] );
         
         SSL_set_bio( ssl[i], bio[i], bio[i] );
         
         int ok=0;
         ok = SSL_accept(ssl[i]);
         
         if ( ok != 1 )
         {
            int err = SSL_get_error(ssl[i],ok);
            char buf[256];
            ERR_error_string_n(err,buf,sizeof(buf));
            
            cerr << "ssl connection failed "<<endl;
            
            bio[i] = NULL;
         }
         
         makeSocketNonBlocking(fd[i]);
      }
      

      // process reads and writes 
      for (int i=0; i<MAX_CONNECTIONS; i++) 
      {
         if ( ssl[i] )
            if (  FD_ISSET( fd[i], &read) ||  SSL_pending(ssl[i])  )
            {
               cerr << "got a message on connection " << i << endl;
            
               char buf[STUN_MAX_MESSAGE_SIZE];
                   
               int ret = SSL_read(ssl[i],buf,sizeof(buf));
               if (ret < 0 )
               {
                  int err = SSL_get_error(ssl[i],ret);
                  switch (err)
                  {
                     case SSL_ERROR_WANT_READ:
                     case SSL_ERROR_WANT_WRITE:
                     case SSL_ERROR_NONE:
                     {
                        cerr << "Got TLS read got condition of " << err  <<endl;
                     }
                     break;
                     default:
                     {
                        char buf[256];
                        ERR_error_string_n(err,buf,sizeof(buf));
                        cerr << "Got TLS read error " << err  << " " << buf  <<endl;
                     }
                     break;
                  }
                  // !cj! - big mem leak here - need to close and cleanup 
                  closesocket( fd[i] );
                  fd[i] = -1;
                  ssl[i]=0;
               }
            
               cerr << "Received message with " << ret << " bytes"<<endl;
            
               StunAddress4 from;
               StunAddress4 myAddr;
               StunAddress4 altAddr;
               StunMessage resp;
               StunAddress4 destination;
               StunAtrString hmacPassword;
               bool changePort;
               bool changeIp;
            
               from.addr = peerip[i];
            
               bool ok = stunServerProcessMsg( buf,ret, 
                                               from, 
                                               myAddr,
                                               altAddr, 
                                               &resp,
                                               &destination,
                                               &hmacPassword,
                                               &changePort,
                                               &changeIp,
                                               true /*verbose*/ );

               if (!ok )
               { 
                  cerr << "Message did not parse - closeing conneciton " << i  <<endl;
                  closesocket( fd[i] );
                  fd[i] = -1;
                  ssl[i]=0;
               }
            
               if (ok)
               {
                  int len = stunEncodeMessage( resp, buf, sizeof(buf), hmacPassword );
           
                  ret = SSL_write(ssl[i],(const char*)buf,len);

                  if (ret < 0 )
                  {
                     int err = SSL_get_error(ssl[i],ret);
                     switch (err)
                     {
                        case SSL_ERROR_WANT_READ:
                        case SSL_ERROR_WANT_WRITE:
                        case SSL_ERROR_NONE:
                        {
                           cerr << "Got TLS write got codition of " << err  <<endl;
                        }
                        break;
                        default:
                        {
                           cerr << "Got TLS write error " << err  <<endl;
                        }
                        break;
                     }
                     // !cj! big mem leak - need to cleanup 
                     closesocket( fd[i] );
                     fd[i] = -1;
                     ssl[i]=0;
                  }
               }
            }
      }
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


