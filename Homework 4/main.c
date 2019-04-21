//
//  main.c
//  Homework 4
//
//  Created by Adam on 4/15/19.
//  Copyright © 2019 Adam Kuniholm. All rights reserved.
//
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define TCP_CONN 1
#define UDP_CONN 2
#define FOREVER_LOOP 1
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

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

typedef struct sockaddr_in saddrin;
typedef struct sockaddr saddr;

typedef struct User {
  int sd;
  saddrin* client;
  char * user_id;
  int connection_type;
  char * buffer;
} User;

User* makeUser(const int sd, saddrin* client) {
  User* user = calloc(1, sizeof(User));
  user->sd = sd;
  user->client = client;
  user->buffer = calloc(BUFFER_SIZE, sizeof(char));
  return NULL;
}

// LOGIN WHO LOGOUT SEND BROADCAST SHARE


//int start_tcp_server(const int port) {
//  const int tcp_sd = socket( PF_INET, SOCK_DGRAM, 0 ); // UDP
//  if ( tcp_sd == -1 ) 
//    perror( "TCP socket() failed" );
////  printf( "MAIN: Listening for TCP connections on port: %d\n", port );
//  return tcp_sd;
//}
//
//int start_udp_server(saddrin * server, const int port) {
//  const int udp_sd = socket( AF_INET, SOCK_DGRAM, 0 ); // UDP
//  if ( udp_sd == -1 ) {
//    perror( "UDP socket() failed" );
//    return -1;
//  }
// 
//  server->sin_family = AF_INET;  /* AF_INET */
//  server->sin_addr.s_addr = htonl( INADDR_ANY );
//  server->sin_port = htons( port );
//  if ( bind( udp_sd, (struct sockaddr *)server, sizeof( *server ) ) < 0 ) {
//    perror( "UDP bind() failed" );
//    return -1;
//  }
//  int length = sizeof( *server );
//  
//  /* call getsockname() to obtain the port number that was just assigned */
//  if ( getsockname( udp_sd, (struct sockaddr *) server, (socklen_t *) &length ) < 0 ) {
//    perror( "UDP getsockname() failed" );
//    return -1;
//  }
//  return udp_sd;
//}
//int initialize_servers(const int port) {
////  int udp_sd;  /* socket descriptor -- this is actually in the fd table! */
////  int tcp_sd;  /* socket descriptor -- this is actually in the fd table! */
//  printf("MAIN: Started server\n");
//  int tcp_sd = start_tcp_server(port); 
//  struct sockaddr_in udp_server;
//  int udp_sd = start_udp_server(&udp_server, port);
//  printf( "MAIN: Listening for TCP connections on port: %d\n", port );
//  printf( "MAIN: Listening for UDP datagrams on port: %d\n", ntohs( udp_server.sin_port ) );
//
//
//  
//  while (FOREVER_LOOP) {
//    break;
//  }
//  close(tcp_sd);
//  close(udp_sd);
//  return EXIT_SUCCESS;
//}

int setup_udp(saddrin* server) {
  int udp_sd = socket(AF_INET, SOCK_DGRAM, 0); 
  if ( bind( udp_sd, (const saddr *) server, sizeof( *server ) ) < 0 ) {
    perror( "UDP bind() failed" );
    return -1;
  }
  return udp_sd;
}

int setup_tcp(saddrin* server, const int port) {
  int tcp_sd = socket(AF_INET, SOCK_STREAM, 0); 
  bzero(&server, sizeof(server)); 
  server->sin_family = AF_INET; 
  server->sin_addr.s_addr = htonl(INADDR_ANY); 
  server->sin_port = htons(port); 
  
  // binding server addr structure to listenfd 
  if ( bind( tcp_sd, (const saddr *)server, sizeof( server ) ) < 0 ) {
    perror( "TCP bind() failed" );
    return -1;
  }
  if (listen(tcp_sd, 10) < 0) {
    perror( "TCP listen() failed" );
    return -1;
  }
  return tcp_sd;
}




int handle_login() {
  return 0;
}

int handle_who() {
  return 0;
}

int handle_logout() {
  return 0;
}

int handle_send() {
  return 0;
}

void handle_connection(void * user) {
  
}

int run_server(const int port) {
  int connfd, nready, max_sd; 
  char buffer[BUFFER_SIZE]; 
  pid_t childpid; 
  fd_set fds; 
  ssize_t n; 
  socklen_t len; 
  saddrin client, server; 
  void sig_chld(int); 
    
  int tcp_sd = setup_tcp(&server, port), udp_sd = setup_udp(&server);
  // clear the descriptor set 
  FD_ZERO(&fds); 
  max_sd = (tcp_sd > udp_sd ? tcp_sd : udp_sd) + 1; 
  while(FOREVER_LOOP) {
    FD_SET(udp_sd, &fds);
    FD_SET(tcp_sd, &fds); 
    nready = select(max_sd, &fds, NULL, NULL, NULL);
    // if tcp socket is readable then handle 
    // it by accepting the connection 
    if (FD_ISSET(tcp_sd, &fds)) { 
      len = sizeof(client); 
      connfd = accept(tcp_sd, (struct sockaddr*)&client, &len);
      User* user = makeUser(tcp_sd, &client);
      user->connection_type = TCP_CONN;
//      handle_connection(user);
      if ((childpid = fork()) == 0) { 
        close(tcp_sd); 
        bzero(buffer, sizeof(buffer)); 
        printf("Message From TCP client: "); 
        read(connfd, buffer, sizeof(buffer)); 
        puts(buffer); 
        //write(connfd, (const char*)message, sizeof(buffer)); 
        close(connfd);  
      } 
      close(connfd); 
    } 
    // if udp socket is readable receive the message. 
    if (FD_ISSET(udp_sd, &fds)) { 
      len = sizeof(client); 
      bzero(buffer, sizeof(buffer)); 
      printf("\nMessage from UDP client: "); 
      n = recvfrom(udp_sd, buffer, sizeof(buffer), 0, 
                   (struct sockaddr*)&client, &len); 
      puts(buffer); 
//      sendto(udpfd, (const char*)message, sizeof(buffer), 0, 
//             (struct sockaddr*)&cliaddr, sizeof(cliaddr)); 
    } 
  }
  return EXIT_SUCCESS;
}



int main(int argc, const char * argv[]) {
  // insert code here...
  // argv[0] port number 
  if (argc < 2) {
    perror( "Usage: server port number" );
    return EXIT_FAILURE;
  }
  unsigned short port = atoi(argv[1]);
//  return initialize_servers(port);
  return run_server(port);
}
