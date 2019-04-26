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
#include <ctype.h>



#define TCP_CONN 1
#define UDP_CONN 2
#define FOREVER_LOOP 1
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 32
#define MAX_USERNAME_LEN 16

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


pthread_t pthread_main;
fd_set fds; 

typedef struct sockaddr_in saddrin;
typedef struct sockaddr saddr;

typedef struct User {
  int sd;
  int fd;
  pthread_t pid;
  char * buffer;
  saddrin* client;
  socklen_t client_size;
  char * user_id;
  int connection_type;
  
} User;

User* makeUser(const int sd, saddrin* client, const int conn_type) {
  User* user = calloc(1, sizeof(User));
  user->sd = sd;
  user->pid = pthread_main;
  user->client = client;
  user->client_size = sizeof(*client);
  user->connection_type = conn_type;
  user->buffer = calloc(BUFFER_SIZE+1, sizeof(char));
  return user;
}
User* makeTCPUser(const int sd, saddrin*client) {
  return makeUser(sd, client, TCP_CONN);
}
User* makeUDPUser(const int sd, saddrin*client) {
  return makeUser(sd, client, UDP_CONN);
}

int sameClient(User * l, User * r) {
  return inet_ntoa(l->client->sin_addr) == inet_ntoa(r->client->sin_addr)
  && ntohs(l->client->sin_port) == ntohs(r->client->sin_port);
}
int sameUserID(User * l, User * r) {
  return l->user_id != NULL && r->user_id != NULL && strcmp(l->user_id, r->user_id) == 0; 
}
int compareUsersLexicographically(const void *l, const void *r) {
  User * lu = (User*) l;
  User * ru = (User*) r;
  return strcmp(lu->user_id, ru->user_id);
}
ssize_t sendUser(User * user, char* message) {
  return sendto(user->fd, message, strlen(message), 0, (saddr*)&user->client, user->client_size);
}
pthread_mutex_t user_mutex = PTHREAD_MUTEX_INITIALIZER;
User** users;


int num_users = 0;
int isChildThread() {
  return pthread_self() != pthread_main; 
}
int isMainThread() {
  return !isChildThread();
}


enum Request{LOGIN, WHO, LOGOUT, SEND, BROADCAST, SHARE, NONE};

enum Request requestType(User* user) {
  if (!user->buffer || strlen(user->buffer) < 4)   return NONE;
  else if (strstr(user->buffer, "LOGIN "))    return LOGIN;
  else if (strstr(user->buffer, "WHO"))       return WHO;
  else if (strstr(user->buffer, "LOGOUT"))   return LOGOUT;
  else if (strstr(user->buffer, "SEND "))   return SEND;
  else if (strstr(user->buffer, "BROADCAST")) return BROADCAST;
  else if (strstr(user->buffer, "SHARE"))     return SHARE;
  return NONE;
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

int setup_udp(saddrin* server, const int port) {
  int udp_sd = socket(AF_INET, SOCK_DGRAM, 0); 
  if (udp_sd < 0) {
    perror( "MAIN: ERROR socket() failed" );
    return -1;
  }
  if ( bind( udp_sd, (const saddr *) server, sizeof( *server ) ) < 0 ) {
    perror( "MAIN: ERROR bind() failed" );
    return -1;
  }
  int l = sizeof(*server);
  if (getsockname(udp_sd,(saddr*)server, (socklen_t *)&l) < 0 ) {
    perror("MAIN: ERROR getsockname() failed");
    return EXIT_FAILURE;
  }
  printf("MAIN: Listening for UDP datagrams on port: %d\n", port);
  return udp_sd;
}

int setup_tcp(saddrin* server, const int port) {
  int tcp_sd = socket(PF_INET, SOCK_STREAM, 0); 
  server->sin_family = PF_INET; 
  server->sin_addr.s_addr = htonl(INADDR_ANY); 
  server->sin_port = htons(port); 
  int l = 1;
  if((setsockopt(tcp_sd, SOL_SOCKET, SO_REUSEADDR, &l, sizeof(l))) < 0) {
    perror("TCP setsockopt() failed");
    return -1;
  }
  if ( bind( tcp_sd, (const saddr *)server, sizeof( *server ) ) < 0 ) {
    perror( "TCP bind() failed" );
    return -1;
  }
  if (listen(tcp_sd, 5) < 0) {
    perror( "TCP listen() failed" );
    return -1;
  }
  printf("MAIN: Listening for TCP connections on port: %d\n", port);

  return tcp_sd;
}



int authenticate(User * user) {
  // Find user
  int userFound = 0==1;
  // If user found, replace current user
  pthread_mutex_lock(&user_mutex);
  for (int i = 0; i < num_users; i++) {
    User * aUser = users[i];
    if (sameClient(user, aUser)) {
      userFound = 0==0;
    }
  }
  pthread_mutex_unlock(&user_mutex);
  return userFound; 
}

int handle_who(User * user) {
  char buffer[BUFFER_SIZE];
  memset(&buffer[0], 0, BUFFER_SIZE);
  strcat(buffer, "OK!\n");
  pthread_mutex_lock(&user_mutex);
  qsort(users, num_users, sizeof(User), compareUsersLexicographically);
  for (int i = 0; i < num_users; i++) {
    strcat(buffer, users[i]->user_id);
    strcat(buffer, "\n");
  }
  pthread_mutex_unlock(&user_mutex);
  sendUser(user, buffer);
  return 0==0;
}

int handle_logout(User * user) {
  // Remove user from group
  // Realloc, copy over nonnull users
  int prev_num_users = num_users;
  pthread_mutex_lock(&user_mutex);
  for (int i = 0; num_users > 0 && i < MAX_CLIENTS; i++) {
    User * curr = users[i];
    if (curr == NULL) 
      continue;
    else if (sameClient(user, curr)) {
      users[i] = users[num_users-1];
      users[num_users-1] = NULL;
      num_users--;
    }
  }
  pthread_mutex_unlock(&user_mutex);
  sendUser(user, "OK!\n");
  return num_users == prev_num_users;
}
typedef struct Message {
  User * recipient;
  User * sender;
  char * text;
} Message;

Message * createMessage(User * user) {
  char name_buff[MAX_USERNAME_LEN+1];
  memset(name_buff, 0, MAX_USERNAME_LEN+1);
  // Walk through the buffer from 5 onwards
  //  int valid = 0==0;
  int reqLen = (int)strlen(user->buffer);
  int loginLen = 0;;
  for (loginLen = 0; loginLen < reqLen && isalpha(user->buffer[loginLen]); loginLen++) {}
  loginLen++;
  int i = 0;
  for (i = loginLen; requestType(user) == SEND &&  i < reqLen && isalpha(user->buffer[i]); i++) 
    // Copy the name over to the name buff;
    name_buff[i-loginLen] = user->buffer[i]; 
  char number[4];
  memset(number, 0, 4);
  //  int user_id_len = i - loginLen;
  int num_start = ++i;
  for (; i < reqLen && isnumber(user->buffer[i]); i++)
    number[i-num_start] = user->buffer[i];
  int msg_len = atoi(number);
  if (msg_len < 1 || msg_len > 990) {
    sendUser(user, "ERROR Invalid msglen\n");
    return NULL;
  }
  
  int msg_start = i + 1;
  user->buffer[msg_start + msg_len] = '\0';
  // FROM <sender-userid> <msglen> <message>\n
  char * msg = calloc(BUFFER_SIZE, sizeof(char));
  memset(msg, 0, BUFFER_SIZE);
  strcat(msg, "FROM ");
  strcat(msg, user->user_id);
  strcat(msg, " ");
  strcat(msg, number);
  strcat(msg, " ");
  strcat(msg, user->buffer + msg_start);
  strcat(msg, "\n");
  User* recipient = NULL;
  for (int u = 0; u < MAX_CLIENTS; u++) {
    if (users[u] == NULL) continue;
    else if (strcmp(users[u]->user_id, name_buff) == 0) 
      recipient = users[u];
  }
  Message * m = calloc(1, sizeof(Message));
  m->sender = user;
  m->recipient = recipient; 
  m->text = msg;
  return m;
}

int handle_send(User * user) {
  if (!authenticate(user)) {
    sendUser(user, "Not logged in!!!\n");
    return 0==1;
  }
  Message* m = createMessage(user);
  
  if (m && m->recipient == NULL) sendUser(user, "ERROR Unknown userid");
  else if (m)                    sendUser(m->recipient, m->text);
  free(m->text);
  free(m);
  
  
  return 0==1;
}

int handle_broadcast(User * user) {
  Message* m = createMessage(user);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (users[i] != NULL) 
      sendUser(users[i], m->text);
  }
  free(m);
  return 0==1;
}
int handle_share(User * user) {
  return 0==1; 
}


int handle_login(User * user) {
  if (authenticate(user)) {
    sendUser(user, "ERROR Already connected\n");
    return 0==1;
  }
  char name_buff[MAX_USERNAME_LEN+1];
  memset(name_buff, 0, MAX_USERNAME_LEN+1);
  // Walk through the buffer from 5 onwards
  int valid = 0==0;
  int loginLen = (int)strlen("LOGIN ");
  int requestLength = (int)strlen(user->buffer);
  int i = 0;
  for (i = loginLen; i < requestLength; i++) {
    char curr = user->buffer[i];
    /* a valid <userid> alphanumeric characters size [4,16].
     if <userid> is invalid, the server responds with an <error-message> of
     “Invalid userid” (do not close)
     Non zero number  If the parameter is an alphabet.  */
    if (i - loginLen >= MAX_USERNAME_LEN) {
      valid = curr == '\n';
      break;
    }
    else if (isalpha(curr) != 0) name_buff[i - loginLen] = curr;
    else if (curr == '\n') break;
    else valid = 0==1;
  }
  if (!valid) {
    sendUser(user, "ERROR Invalid userid\n");
    free(name_buff);
    return 0==1;
  } else {
    free(user->user_id);
    user->user_id = calloc(i-loginLen+1, sizeof(char));
    strcpy(user->user_id, name_buff);
  }
  if (isMainThread())
    printf("MAIN: Rcvd LOGIN request for userid %s\n", user->user_id);
  else 
    printf("CHILD %d: Rcvd LOGIN request for userid %s\n", (int)pthread_self(), user->user_id);
  
  pthread_mutex_lock(&user_mutex);
  int found = 0==1;
  for (int i = 0; !found && i < num_users; i++) {
    User* aUser = users[i];
    int clientsEqual = sameClient(user, aUser);
    int userIDsEqual = sameUserID(user, aUser);
    found = userIDsEqual || clientsEqual;
  }
  pthread_mutex_unlock(&user_mutex);
  if (found) sendUser(user, "ERROR Already connected\n");
  else if (num_users < MAX_CLIENTS){
    pthread_mutex_lock(&user_mutex);
    users[num_users++] = user;
    pthread_mutex_unlock(&user_mutex);
    sendUser(user, "OK!\n");
  } else {
    sendUser(user, "ERROR Too many users\n");
  } 
  return 0==0;
}



ssize_t readFromUser(User * user) {
  memset(user->buffer, 0, BUFFER_SIZE);
  ssize_t n = read(user->fd, user->buffer, BUFFER_SIZE);
  printf("Message from TCP client: \"%s\" (size %lu)\n", user->buffer, strlen(user->buffer)); 
  return n;
}


void* handle_tcp_connection(void* argv) {
 
  User* user = argv;
  while (user != NULL && (readFromUser(user)<-1 || FOREVER_LOOP)) 
    switch(requestType(user)) {
      case LOGIN:
        handle_login(user);
        break;
      case WHO:
        handle_who(user); 
        break;
      case LOGOUT:
        handle_logout(user);
        break;
      case BROADCAST:
        handle_broadcast(user);
        break;
      case SHARE:
        handle_share(user);
      case SEND:
        handle_send(user);
        break;
      default:
        break;
    }
  
  close(user->fd);
  free(user);
  pthread_exit(NULL);
  return NULL;
  
}

void* handle_udp_connection(void* argv) {
  User* user = argv;
  printf("Message from UDP client: %s", user->buffer);
  pthread_exit(NULL);
  return NULL;
}


int run_server(const int port) {
  printf("MAIN: Started server\n");
  users = calloc(MAX_CLIENTS, sizeof(User));
  num_users = 0;
  int max_sd; 
  socklen_t l; 
  saddrin* client = calloc(1, sizeof(saddrin));
  saddrin* server = calloc(1, sizeof(saddrin));
  void sig_chld(int); 
  int tcp_sd = setup_tcp(server, port), udp_sd = setup_udp(server, port);
  if (tcp_sd < 0 || udp_sd < 0) return EXIT_FAILURE; 
  // clear the descriptor set 
  FD_ZERO(&fds); 
  while(FOREVER_LOOP) {
    FD_ZERO(&fds);
    FD_SET(udp_sd, &fds);
    FD_SET(tcp_sd, &fds);
    max_sd = (tcp_sd > udp_sd ? tcp_sd : udp_sd) + 1; 
    if (select(max_sd, &fds, NULL, NULL, NULL) == 0) continue;
    
    // TCP //////////////////////
    if (FD_ISSET(tcp_sd, &fds)) { 
      l = sizeof(client); 
      User* user = makeTCPUser(tcp_sd, client);
      user->fd = accept(tcp_sd, (saddr*)client, &l);
      printf("MAIN: Rcvd incoming TCP connection from: %s\n",
             inet_ntoa((struct in_addr)client->sin_addr));
      pthread_create(&user->pid, NULL, handle_tcp_connection, user);
    } 
    
    // UDP //////////////////////
    if (FD_ISSET(udp_sd, &fds)) { 
      socklen_t l = sizeof(client);
      User* user = makeUDPUser(udp_sd, client);
      
      ssize_t received = recvfrom(udp_sd, user->buffer, BUFFER_SIZE,0, (saddr*)client, (socklen_t*)&l);
      if (received < 0) { 
        perror("MAIN: ERROR recvfrom() failed");
      }
      else {
        printf("MAIN: Rcvd incoming UDP datagram from: %s\n", inet_ntoa(client->sin_addr));
//        user->buffer[BUFFER_SIZE] = '\0';
//        handle_udp_connection(user);

      }
      free(user);

//      FD_SET(udp_sd, &fds);
//      bzero(buffer, sizeof(buffer)); 
//      
//      printf("\nMessage from UDP client: "); 
//      n = recvfrom(udp_sd, buffer, sizeof(buffer), 0, 
//                   (struct sockaddr*)&client, &len); 
//      puts(buffer); 
//      sendto(udpfd, (const char*)message, sizeof(buffer), 0, 
//             (struct sockaddr*)&cliaddr, sizeof(cliaddr)); 
    }
    FD_ZERO(&fds); 

  }
  return EXIT_SUCCESS;
}



int main(int argc, const char * argv[]) {
  pthread_main = pthread_self();
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
