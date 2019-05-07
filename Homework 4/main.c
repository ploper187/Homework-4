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
#define MIN_USERNAME_LEN 4

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
  char * id;
  int conn_type;
} User;

User* makeUser(const int sd, saddrin* client, const int conn_type) {
  User* user = calloc(1, sizeof(User));
  user->sd = sd;
  user->pid = pthread_main;
  user->client = client;
  user->client_size = sizeof(*client);
  user->conn_type = conn_type;
  user->buffer = calloc(BUFFER_SIZE+1, sizeof(char));
  return user;
}
User* makeTCPUser(const int sd, saddrin*client) {
  return makeUser(sd, client, TCP_CONN);
}
User* makeUDPUser(const int sd, saddrin*client) {
  return makeUser(sd, client, UDP_CONN);
}

int num_users = 0;
int isChildThread() {
  return pthread_self() != pthread_main; 
}
int isMainThread() {
  return !isChildThread();
}

int sameClient(User * l, User * r) {
  return inet_ntoa(l->client->sin_addr) == inet_ntoa(r->client->sin_addr)
  && ntohs(l->client->sin_port) == ntohs(r->client->sin_port);
}
int sameUserID(User * l, User * r) {
  return l->id != NULL && r->id != NULL && strcmp(l->id, r->id) == 0; 
}
int compareUsersLexicographically(const void *l, const void *r) {
  User * lu = (User*) l;
  User * ru = (User*) r;
  return (lu && ru) && (lu->id && ru->id) && strcmp(lu->id, ru->id);
}
ssize_t sendUser(User * user, char* message) {
  ssize_t rc = sendto(user->fd, message, strlen(message), 0, (saddr*)&user->client, user->client_size);
  return rc;
}
ssize_t readFromUser(User * user) {
  memset(user->buffer, 0, BUFFER_SIZE);
  ssize_t n = read(user->fd, user->buffer, BUFFER_SIZE);
  return n;
}

pthread_mutex_t user_mutex = PTHREAD_MUTEX_INITIALIZER;
User** users;

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

int setup_udp(saddrin* server, const int port) {
  struct timeval to;
  to.tv_sec = 3;
  to.tv_usec = 500;
  int udp_sd = socket(AF_INET, SOCK_DGRAM, 0); 
  if (udp_sd < 0) {
    perror( "MAIN: ERROR socket() failed" );
    return -1;
  }
  server->sin_family = AF_INET;
  server->sin_addr.s_addr = htonl(INADDR_ANY);
  server->sin_port = htons(port);
  if (setsockopt(udp_sd, SOL_SOCKET, SO_REUSEADDR, &to, sizeof(to)) < 0) {
    perror("MAIN: ERROR setsockopt() failed");
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
  if (isMainThread())
    printf("MAIN: Rcvd WHO request\n");
  else 
    printf("CHILD %d: Rcvd WHO request\n", (int)pthread_self());
  char buffer[BUFFER_SIZE];
  memset(&buffer[0], 0, BUFFER_SIZE);
  strcat(buffer, "OK!\n");
  pthread_mutex_lock(&user_mutex);
  qsort(users, num_users, sizeof(User), compareUsersLexicographically);
  for (int i = 0; i < num_users; i++) {
    strcat(buffer, users[i]->id);
    strcat(buffer, "\n");
  }
  pthread_mutex_unlock(&user_mutex);
  sendUser(user, buffer);
  return 0==0;
}
int handle_logout_silent(User* user) {
  int prev_num_users = num_users;
  pthread_mutex_lock(&user_mutex);
  for (int i = 0; num_users > 0 && i < MAX_CLIENTS; i++) {
    User * curr = users[i];
    if (curr == NULL) 
      continue;
    else if (sameUserID(user, curr)) {
      users[i] = users[num_users-1];
      users[num_users-1] = NULL;
      num_users--;
    }
  }
  pthread_mutex_unlock(&user_mutex);
  return num_users == prev_num_users;
}
int handle_logout(User * user) {
  if (isMainThread())
    printf("MAIN: Rcvd LOGOUT request\n");
  else 
    printf("CHILD %d: Rcvd LOGOUT request\n", (int)pthread_self());
  // Remove user from group
  // Realloc, copy over nonnull users
  int success = handle_logout_silent(user);
  sendUser(user, "OK!\n");
  return success;
}


typedef struct Message {
  User * recipient;
  User * sender;
  char * to_id;
  char * text;
  int text_len;
} Message;

Message * createMessage(User * user) {
  char name_buff[MAX_USERNAME_LEN+1];
  memset(name_buff, 0, MAX_USERNAME_LEN+1);
  // Walk through the buffer from 5 onwards
  //  int valid = 0==0;
  int reqLen = (int)strlen(user->buffer);
  int loginLen = 0;
  for (loginLen = 0; \
       loginLen < reqLen && isalpha(user->buffer[loginLen]); \
       loginLen++) {}
  loginLen++;
  int i = 0;
  for (i = loginLen; \
       requestType(user) == SEND &&  i < reqLen && isalpha(user->buffer[i]);\
       i++) 
    // Copy the name over to the name buff;
    name_buff[i-loginLen] = user->buffer[i]; 
  char number[4];
  memset(number, 0, 4);
  //  int user_id_len = i - loginLen;
  int num_start = i + (isdigit(user->buffer[i]) ? 0 : 1);
  for (i = num_start; i < reqLen && isdigit(user->buffer[i]); i++)
    number[i-num_start] = user->buffer[i];
  int msg_len = atoi(number);
  if (!user->id) {
    printf("ERROR not logged in!");
    return NULL;
  }
  if (msg_len < 1 || msg_len > 990) {
    printf("Message number %s\n", number);
    sendUser(user, "ERROR Invalid msglen\n");
    return NULL;
  }
  
  int msg_start = i + 1;
  if (msg_len + msg_start < reqLen)
    user->buffer[msg_start + msg_len] = '\0';
  else 
    user->buffer[reqLen - 1] = '\0';
  // FROM <sender-userid> <msglen> <message>\n
  char * msg = calloc(BUFFER_SIZE, sizeof(char));
  memset(msg, 0, BUFFER_SIZE);
  strcat(msg, "FROM ");
  strcat(msg, user->conn_type == TCP_CONN ? user->id : "UDP-Client");
  strcat(msg, " ");
  strcat(msg, number);
  strcat(msg, " ");
  strcat(msg, user->buffer + msg_start);
  strcat(msg, "\n");
  User* recipient = NULL;
  for (int u = 0; u < MAX_CLIENTS; u++) {
    if (users[u] == NULL) continue;
    else if (strcmp(users[u]->id, name_buff) == 0) 
      recipient = users[u];
  }
  Message * m = calloc(1, sizeof(Message));
  m->to_id = calloc(strlen(name_buff) + 1, sizeof(char));
  strcpy(m->to_id, name_buff);
  m->sender = user;
  m->recipient = recipient; 
  m->text = msg;
  m->text_len = msg_len;
  return m;
}

int handle_send(User * user) {
  Message* m = createMessage(user);
  
  if (isMainThread())
    printf("MAIN: Rcvd SEND request to userid %s\n", m->to_id);
  else if (m) 
    printf("CHILD %d: Rcvd SEND request to userid %s\n", (int)pthread_self(), m->to_id);
  if (m && m->recipient == NULL) {
    sendUser(user, "ERROR Unknown userid");
  }
  else if (m) {
    sendUser(user, "OK!\n");
    sendUser(m->recipient, m->text);
  }
  free(m);
  return 0==1;
}

int handle_broadcast(User * user) {
  if (isMainThread())
    printf("MAIN: Rcvd BROADCAST request\n");
  else 
    printf("CHILD %d: Rcvd BROADCAST request\n", (int)pthread_self());
  Message* m = createMessage(user);
  
  for (int i = 0; m && i < MAX_CLIENTS; i++) 
    if (users[i]) 
      sendUser(users[i], m->text);
  
  free(m);
  return 0==1;
}
// SHARE <recipient-userid> <filelen>\n
int handle_share(User * user) {
  if (isMainThread())
    printf("MAIN: Rcvd SHARE request\n");
  else 
    printf("CHILD %d: Rcvd SHARE request\n", (int)pthread_self());
  Message * m = createMessage(user);
  if (!(m->recipient && m->sender && m->sender)) 
    return 0==1; 
  
  // OK!\n
  sendUser(user, "OK!\n");
  // SHARE <sender-userid> <filelen>\n
  char recipient_response[BUFFER_SIZE];
  char number[4];
  sprintf(number, "%d", m->text_len);
  memset(&recipient_response, 0, BUFFER_SIZE);
  strcat(recipient_response, "SHARE ");
  strcat(recipient_response, user->id);
  strcat(recipient_response, " ");
  strcat(recipient_response,number);
  strcat(recipient_response, "\n");
  sendUser(m->recipient, recipient_response);
  
  int chars_left = m->text_len;
  while (chars_left > 0) {
    ssize_t n = readFromUser(user);
    if (n == BUFFER_SIZE || chars_left == n) {
      sendUser(m->recipient, user->buffer);
    } else
      break;
    chars_left -= n;
  }
  return 0==1; 
}


int handle_login(User * user) {
  char name_buff[MAX_USERNAME_LEN+1];
  memset(name_buff, 0, MAX_USERNAME_LEN+1);
  int isValid = 0==0;
  int loginLen = (int)strlen("LOGIN ");
  int requestLength = (int)strlen(user->buffer);
  int i = 0;
  for (i = loginLen; isValid && i < requestLength; i++) {
    char curr = user->buffer[i];
    /* a valid <userid> alphanumeric characters size [4,16].
     if <userid> is invalid, the server responds with an <error-message> of
     “Invalid userid” (do not close)
     Non zero number  If the parameter is an alphabet.  */
    if (i - loginLen >= MAX_USERNAME_LEN) isValid = curr == '\n';
    else if (isalpha(curr) != 0 || isdigit(curr) != 0) name_buff[i - loginLen] = curr;
    else if (curr == '\n') break;
    else isValid = 0==1;
  }
  isValid = isValid && (i - loginLen >= MIN_USERNAME_LEN);
  if (!isValid) {
    if (isMainThread())
      printf("MAIN: Sent ERROR (Invalid userid)\n");
    else 
      printf("CHILD %d: Sent ERROR (Invalid userid)\n", (int)pthread_self());
    sendUser(user, "ERROR Invalid userid\n");
    return 0==1;
  } else {
    free(user->id);
    user->id = calloc(i-loginLen+1, sizeof(char));
    strcpy(user->id, name_buff);
  }
  if (isMainThread())
    printf("MAIN: Rcvd LOGIN request for userid %s\n", user->id);
  else 
    printf("CHILD %d: Rcvd LOGIN request for userid %s\n", (int)pthread_self(), user->id);
  
  pthread_mutex_lock(&user_mutex);
  int found = 0==1;
  for (int i = 0; !found && i < num_users; i++) {
    User* aUser = users[i];
    int userIDsEqual = sameUserID(user, aUser);
    found = userIDsEqual;
  }
  pthread_mutex_unlock(&user_mutex);
  if (found) { 
    if (isMainThread())
      printf("MAIN: Sent ERROR (Already connected)\n");
    else 
      printf("CHILD %d: Sent ERROR (Already connected)\n", (int)pthread_self());
    sendUser(user, "ERROR Already connected\n");
  }
  else if (num_users < MAX_CLIENTS){
    pthread_mutex_lock(&user_mutex);
    users[num_users++] = user;
    pthread_mutex_unlock(&user_mutex);
    sendUser(user, "OK!\n");
  } else
    sendUser(user, "ERROR Too many users\n");
  return 0==0;
}



void handle_tcp_connection(User * user) {
  while (user != NULL && readFromUser(user) != 0 && FOREVER_LOOP) 
    switch(requestType(user)) {
      case BROADCAST: handle_broadcast(user); break;
      case LOGOUT:    handle_logout(user);    break;
      case SHARE:     handle_share(user);     break;
      case LOGIN:     handle_login(user);     break;
      case SEND:      handle_send(user);      break;
      case WHO:       handle_who(user);       break;
      default: 
        break;
    }
}

void handle_udp_connection(User * user) {
  switch(requestType(user)) {
    case BROADCAST: handle_broadcast(user); break;
    case WHO:       handle_who(user);       break;
      
    case LOGIN:  sendUser(user, "LOGIN not supported over UDP");
      break;
    case LOGOUT: sendUser(user, "LOGOUT not supported over UDP");     
      break;
    case SHARE:  sendUser(user, "SHARE not supported over UDP");
      break;
    case SEND:   sendUser(user, "SEND not supported over UDP");
    break;
    default:
      break;
  }
}

void* handle_connection(void* argv) {
  User* user = argv;
  switch (user->conn_type) {
    case TCP_CONN: handle_tcp_connection(user); break;
    case UDP_CONN: handle_udp_connection(user); break;
  }
  if (isMainThread())
    printf("MAIN: Client disconnected\n");
  else 
    printf("CHILD %d: Client disconnected\n", (int)pthread_self());
  pthread_exit(NULL);
  return NULL;
}

int run_server(const int port) {
  printf("MAIN: Started server\n");
  users = calloc(MAX_CLIENTS, sizeof(User));
  num_users = 0;
  socklen_t l; 
  saddrin* client = calloc(1, sizeof(saddrin));
  saddrin* server = calloc(1, sizeof(saddrin));
  struct timeval to;
  to.tv_sec = 3;
  to.tv_usec = 500;
  int tcp_sd = setup_tcp(server, port), udp_sd = setup_udp(server, port);
  if (tcp_sd < 0 || udp_sd < 0) return EXIT_FAILURE; 
  // clear the descriptor set 
  FD_ZERO(&fds); 
  while(FOREVER_LOOP) {
    FD_ZERO(&fds);
    FD_SET(udp_sd, &fds);
    FD_SET(tcp_sd, &fds);
    if (select(FD_SETSIZE, &fds, NULL, NULL, &to) == 0) continue;
  
    // TCP //////////////////////
    if (FD_ISSET(tcp_sd, &fds)) { 
      l = sizeof(client); 
      User* user = makeTCPUser(tcp_sd, client);
      user->fd = accept(tcp_sd, (saddr*)client, &l);
      printf("MAIN: Rcvd incoming TCP connection from: %s\n",
             inet_ntoa((struct in_addr)client->sin_addr));
      pthread_create(&user->pid, NULL, handle_connection, user);
    } 
    
    // UDP //////////////////////
    if (FD_ISSET(udp_sd, &fds)) { 
      socklen_t l = sizeof(client);
      User* user = makeUDPUser(udp_sd, client);
      user->fd = udp_sd;
      ssize_t received = recvfrom(udp_sd, user->buffer, BUFFER_SIZE,0, (saddr*)client, (socklen_t*)&l);
      if (received < 0)  
        perror("MAIN: ERROR recvfrom() failed");
      else {
        printf("MAIN: Rcvd incoming UDP datagram from: %s\n", 
               inet_ntoa(client->sin_addr));
        user->buffer[BUFFER_SIZE] = '\0';
        handle_connection(user);
      }
      free(user);
    } 
  }
  return EXIT_SUCCESS;
}



int main(int argc, const char * argv[]) {
  setvbuf( stdout, NULL, _IONBF, 0 );
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
