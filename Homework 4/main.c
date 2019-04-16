//
//  main.c
//  Homework 4
//
//  Created by Adam on 4/15/19.
//  Copyright © 2019 Adam Kuniholm. All rights reserved.
//
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>


#define FOREVER_LOOP 1

/*
 a multi-threaded chat server using sockets.
 Clients will be able to send and receive short text
 messages from one another. Clients will also be able to send and receive files, including both text
 and binary files (e.g., images) with arbitrarily large sizes
 
 via TCP or UDP via the port number- first command-line argument. 
 
 For TCP, clients connect to this port number (i.e., the TCP listener
 port). 
 
 For UDP, clients send datagram(s) to this port number
 
 To support both TCP and UDP at the same time, you must use the select() system call in the
 main thread to poll for incoming TCP connection requests and UDP datagrams
 
 Your server must not be a single-threaded iterative server. Instead, your server must allocate a
 child thread for each TCP connection. Further, your server must be parallelized to the extent
 possible. As such, be sure you handle all potential synchronization issues.
 
  only handle streams of bytes as opposed to any language-specific structures.
 
  use netcat to test your server; do not use telnet.
 
 Your server must support at least 32 concurrently connected clients (i.e., TCP connections, with
 each connection corresponding to a child thread)
 
 For TCP, use a dedicated child thread to handle each TCP connection. In other words, after the
 accept() call, immediately create a child thread to handle that connection, thereby enabling the
 main thread to loop back around and call select() again. (v1.1) For TCP, you are only guaranteed
 to receive data up to and including the first newline character in the first packet received; therefore,
 expect to implement multiple read() calls.
 
 
 
 
 The application-layer protocol between client and server is a line-based protocol. Streams of bytes
 (i.e., characters) are transmitted between clients and your server. Note that all commands are
 specified in upper-case and end with a newline ('\n') character, as shown below.
 In general, when the server receives a request, it responds with either a four-byte “OK!\n” response
 or an error. When an error occurs, the server must respond with:
 
 LOGIN <userid>\n 
 valid <userid> alphanumeric characters length [4,16].
 Upon receiving a LOGIN request, if successful, the server sends the four-byte “OK!\n” string.
 
 If the given <userid> is already connected via TCP, the server responds with an <error-message>
 of “Already connected”; if <userid> is invalid, the server responds with an <error-message> of
 “Invalid userid” (and in both cases keeps the TCP connection open).
 
 WHO
 obtain a list of all users currently active within the chat server.
 Server Response e.g. "OK!\nMorty\nRick\nShirley\nSummer\nmeme\nqwerty\n"
 
 Note that the WHO command must be supported for both TCP and UDP.
 
 LOGOUT
 server is required to send an “OK!\n” response
 connection should stay open (until the remote side closes the connection).
 
 SEND
 A user connected via TCP may attempt to send a private message to another user via the SEND
 SEND <recipient-userid> <msglen>\n<message>
 To be a valid SEND request, the <recipient-userid> must be a currently active user and the
 <msglen> (i.e., length of the <message> portion) must be an integer in the range [1,990].
 Note that the <message> can contain any bytes whatsoever. You may assume that the number of
 bytes will always match the given <msglen> value.
 
 */


#include <stdio.h>

int main(int argc, const char * argv[]) {
  // insert code here...
  // argv[0] port number 
  if (argc < 2) {
    perror( "Usage: server port number" );
    return EXIT_FAILURE;
  }
  unsigned short port = atoi(argv[1]);
  int udp_sd;  /* socket descriptor -- this is actually in the fd table! */
  udp_sd = socket( AF_INET, SOCK_DGRAM, 0 ); // UDP
  if ( udp_sd == -1 )
  {
    perror( "UDP socket() failed" );
    return EXIT_FAILURE;
  }
  /* socket structures */
  struct sockaddr_in udp_server;
  udp_server.sin_family = AF_INET;  /* AF_INET */
  udp_server.sin_addr.s_addr = htonl( INADDR_ANY );
  udp_server.sin_port = htons( port );
  if ( bind( udp_sd, (struct sockaddr *) &udp_server, sizeof( udp_server ) ) < 0 )
  {
    perror( "UDP bind() failed" );
    return EXIT_FAILURE;
  }
  int length = sizeof( udp_server );
  
  /* call getsockname() to obtain the port number that was just assigned */
  if ( getsockname( udp_sd, (struct sockaddr *) &udp_server, (socklen_t *) &length ) < 0 )
  {
    perror( "UDP getsockname() failed" );
    return EXIT_FAILURE;
  }
  printf( "UDP server at port number %d\n", ntohs( udp_server.sin_port ) );

  
  int tcp_sd;  /* socket descriptor -- this is actually in the fd table! */
  tcp_sd = socket( PF_INET, SOCK_DGRAM, 0 ); // UDP
  if ( tcp_sd == -1 )
  {
    perror( "TCP socket() failed" );
    return EXIT_FAILURE;
  }
  
  while (FOREVER_LOOP) {
    break;
    
    
  }
  printf("Exiting\n");
  return 0;
}
