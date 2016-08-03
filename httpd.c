/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 *      I am determined that I will eventually use GNU autoconf to
 *      handle all this junk for you!  (I'll also handle threading
 *      correctly; it seems even pthread isn't the same between Linux
 *      and Solaris.)
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
/* Commented for compiling in Linux.
#include <pthread.h>
 */
#include <sys/wait.h>
#include <stdlib.h>

/**
 * Test whether given character is space.
 *
 * @param {char} x - Character.
 *
 * @return Boolean.
 */
#define ISspace(x) isspace((int)(x))

// `Server` header line
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

// Declare functions
void accept_request(int);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client)
{
 // Input buffer
 char buf[1024];

 // Number of bytes read
 int numchars;

 // HTTP method name buffer
 char method[255];

 // URL buffer
 char url[255];

 // Path buffer
 char path[512];

 // Buffer indexes
 size_t i, j;

 // File status structure
 struct stat st;

 // Whether is CGI mode
 int cgi = 0;      /* becomes true if server decides this is a CGI
                    * program */

 // Query string pointer
 char *query_string = NULL;

 // Read the HTTP start line into the input buffer.
 // Get the number of bytes read.
 numchars = get_line(client, buf, sizeof(buf));

 // Buffer indexes
 i = 0; j = 0;

 // For the input buffer's each byte.
 //     If the byte is not space,
 //     and the method name buffer is not full.
 //         Enter loop body.
 while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
 {
  // Copy the byte to the method name buffer
  method[i] = buf[j];

  // Incremnt buffer indexes
  i++; j++;
 }

 // Add nul to string end
 method[i] = '\0';

 // If the method name is not `GET`,
 // and the method name is not `POST`.
 if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
 {
  // Send 501 response
  unimplemented(client);

  // Return
  return;
 }

 // If the method name is `GET`,
 // or the method name is `POST`.

 // If the method name is `POST`,
 if (strcasecmp(method, "POST") == 0)
  // Set the CGI mode be true
  cgi = 1;

 // Set index `i` be 0
 i = 0;

 // For the remaining input buffer's each byte.
 //     If the byte is space,
 //     and the buffer is not run out.
 while (ISspace(buf[j]) && (j < sizeof(buf)))
  // Increment the buffer index to ignore the space
  j++;

 // For the remaining buffer's each byte.
 //     If the byte is not space,
 //     and the url buffer is not full,
 //     and the input buffer is not run out.
 //         Enter loop body.
 while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
 {
  // Copy the byte to the url buffer
  url[i] = buf[j];

  // Increment buffer indexes
  i++; j++;
 }

 // Add nul to string end
 url[i] = '\0';

 // If the method name is `GET`
 if (strcasecmp(method, "GET") == 0)
 {
  // Set the query sting be the url
  query_string = url;

  // For the url's each byte.
  //     If the byte is not `?`,
  //     and the url is not run out.
  //         Enter loop body.
  while ((*query_string != '?') && (*query_string != '\0'))
   // Increment the query sting pointer to ignore the byte
   query_string++;

  // If the byte is `?`,
  // it means the url has query string.
  if (*query_string == '?')
  {
   // Set the CGI mode be true
   cgi = 1;

   // Set the byte be nul to remove the query string from the url string
   *query_string = '\0';

   // Increment the query string pointer to point to the query string
   query_string++;
  }
 }

 // Add `htdocs` to the url's path as prefix.
 // Add the prefixed path to the path buffer.
 sprintf(path, "htdocs%s", url);

 // If the path string ends with `/`,
 // it means it is a directory path.
 if (path[strlen(path) - 1] == '/')
  // Append `index.html` to the path string
  strcat(path, "index.html");

 // If the path not exists,
 // it means 404 error.
 if (stat(path, &st) == -1) {
  // While the last read is successful,
  // and the buffer's first byte is not `\n`,
  // it means the input data's headers part is not run out.
  while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
   // Read one header line
   numchars = get_line(client, buf, sizeof(buf));

  // Send 404 response
  not_found(client);
 }

 // If the path exists
 else
 {
  // If the path is directory path
  if ((st.st_mode & S_IFMT) == S_IFDIR)
   // Append `/index.html` to the path string
   strcat(path, "/index.html");

  // If the path is executable
  if ((st.st_mode & S_IXUSR) ||
      (st.st_mode & S_IXGRP) ||
      (st.st_mode & S_IXOTH)    )
   // Set the CGI mode be true
   cgi = 1;

  // If the CGI mode is not true
  if (!cgi)
   // Serve file on the path
   serve_file(client, path);

  // If the CGI mode is true
  else
   // Execute program file on the path
   execute_cgi(client, path, method, query_string);
 }

 // Close the request socket
 close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
 // Buffer
 char buf[1024];

 // Add HTTP start line to the buffer
 sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");

 // Send
 send(client, buf, sizeof(buf), 0);

 // Add `Content-Type` header to the buffer
 sprintf(buf, "Content-type: text/html\r\n");

 // Send
 send(client, buf, sizeof(buf), 0);

 // Add empty line to the buffer to mean end of headers part
 sprintf(buf, "\r\n");

 // Send
 send(client, buf, sizeof(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "<P>Your browser sent a bad request, ");

 // Send
 send(client, buf, sizeof(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "such as a POST without a Content-Length.\r\n");

 // Send
 send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
 // Buffer
 char buf[1024];

 // Read file chunk to the buffer
 fgets(buf, sizeof(buf), resource);

 // While the file is not run out
 while (!feof(resource))
 {
  // Send the file chunk
  send(client, buf, strlen(buf), 0);

  // Read file chunk to the buffer
  fgets(buf, sizeof(buf), resource);
 }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
 // Buffer
 char buf[1024];

 // Add HTTP start line to the buffer
 sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add `Content-Type` header to the buffer
 sprintf(buf, "Content-type: text/html\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add empty line to the buffer to mean end of headers part
 sprintf(buf, "\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add HTML page to the buffer
 sprintf(buf, "<P>Error prohibited CGI execution.\r\n");

 // Send
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
 // Print message
 perror(sc);

 // Exit with error code
 exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string)
{
 // Buffer
 char buf[1024];

 // Output pipe's read and write FDs.
 // Output pipe is for the child process to send data to the parent process.
 int cgi_output[2];

 // Input pipe's read and write FDs.
 // Input pipe is for the parent process to send data to the child process.
 int cgi_input[2];

 // Child process PID
 pid_t pid;

 // `waitpid`'s status
 int status;

 // Loop index
 int i;

 // Byte read
 char c;

 // Number of bytes read
 int numchars = 1;

 // `Content-Length` header's value
 int content_length = -1;

 // Set the buffer's initial value be "A\0"
 buf[0] = 'A'; buf[1] = '\0';

 // If the method name is `GET`
 if (strcasecmp(method, "GET") == 0)
  // While the last read is successful,
  // and the buffer's first byte is not `\n`,
  // it means the input data's headers part is not run out.
  while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
   // Read one header line
   numchars = get_line(client, buf, sizeof(buf));

 // If the method name is not `GET`,
 // it means `POST`.
 else    /* POST */
 {
  // Read one header line
  numchars = get_line(client, buf, sizeof(buf));

  // While the last read is successful,
  // and the buffer's first byte is not `\n`,
  // it means the input data's headers part is not run out.
  while ((numchars > 0) && strcmp("\n", buf))
  {
   // Add nul character after "Content-Length:" for the `strcasecmp` below
   buf[15] = '\0';

   // If the header key is `Content-Length`
   if (strcasecmp(buf, "Content-Length:") == 0)
    // Get the header value
    content_length = atoi(&(buf[16]));

   // Read one header line
   numchars = get_line(client, buf, sizeof(buf));
  }

  // If content length is not valid
  if (content_length == -1) {
   // Send 400 response
   bad_request(client);

   // Return
   return;
  }
 }

 // Add HTTP start line to the buffer
 sprintf(buf, "HTTP/1.0 200 OK\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Create output pipe.
 //
 // If creating output pipe is not successful.
 if (pipe(cgi_output) < 0) {
  // Send 500 response
  cannot_execute(client);

  // Return
  return;
 }

 // If creating output pipe is successful.

 // Create input pipe.
 //
 // If creating input pipe is not successful.
 if (pipe(cgi_input) < 0) {
  // Send 500 response
  cannot_execute(client);

  // Return
  return;
 }

 // If creating input pipe is successful.

 // Fork child process.
 //
 // If forking child process is not successful.
 if ( (pid = fork()) < 0 ) {
  // Send 500 response
  cannot_execute(client);

  // Return
  return;
 }

 // If forking child process is successful.

 // If this is the child process
 if (pid == 0)  /* child: CGI script */
 {
  // REQUEST_METHOD buffer
  char meth_env[255];

  // QUERY_STRING buffer
  char query_env[255];

  // CONTENT_LENGTH buffer
  char length_env[255];

  // Set stdout be the output pipe's write end
  dup2(cgi_output[1], 1);

  // Set stdin be the input pipe's read end
  dup2(cgi_input[0], 0);

  // Close the output pipe's read end because the output pipe is for the child
  // process to send data to the parent process
  close(cgi_output[0]);

  // Close the input pipe's write end because the input pipe is for the parent
  // process to send data to the child process
  close(cgi_input[1]);

  // Add given method name to the REQUEST_METHOD buffer
  sprintf(meth_env, "REQUEST_METHOD=%s", method);

  // Set REQUEST_METHOD environment variable
  putenv(meth_env);

  // If the method name is `GET`
  if (strcasecmp(method, "GET") == 0) {
   // Add given query string to the QUERY_STRING buffer
   sprintf(query_env, "QUERY_STRING=%s", query_string);

   // Set QUERY_STRING environment variable
   putenv(query_env);
  }

  // If the method name is not `GET`,
  // it means `POST`.
  else {   /* POST */
   // Add content length to the CONTENT_LENGTH buffer
   sprintf(length_env, "CONTENT_LENGTH=%d", content_length);

   // Set CONTENT_LENGTH environment variable
   putenv(length_env);
  }

  // Run the CGI program in the current process
  execl(path, path, NULL);

  // Exit without error
  exit(0);

 // If this is the parent process
 } else {    /* parent */
  // Close the output pipe's write end because the output pipe is for the child
  // process to send data to the parent process
  close(cgi_output[1]);

  // Close the input pipe's read end because the input pipe is for the parent
  // process to send data to the child process
  close(cgi_input[0]);

  // If the method name is `POST`
  if (strcasecmp(method, "POST") == 0)
   // For input data's body part's each byte
   for (i = 0; i < content_length; i++) {
    // Read one byte
    recv(client, &c, 1, 0);

    // Write the byte to the child process
    write(cgi_input[1], &c, 1);
   }

  // Read one byte from the child process.
  //
  // If the read is successful.
  //     Enter loop body.
  while (read(cgi_output[0], &c, 1) > 0)
   // Send the byte to client
   send(client, &c, 1, 0);

  // Close the output pipe's read end
  close(cgi_output[0]);

  // Close the input pipe's write end
  close(cgi_input[1]);

  // Wait the child process to finish
  waitpid(pid, &status, 0);
 }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
 // Buffer index
 int i = 0;

 // Byte read
 char c = '\0';

 // Number of bytes read
 int n;

 // While the buffer is not full,
 // and the byte is not `\n`.
 while ((i < size - 1) && (c != '\n'))
 {
  // Read one byte
  n = recv(sock, &c, 1, 0);
  /* DEBUG printf("%02X\n", c); */

  // If the read is successful
  if (n > 0)
  {
   // If the byte is `\r`
   if (c == '\r')
   {
    // Peek the next byte
    n = recv(sock, &c, 1, MSG_PEEK);
    /* DEBUG printf("%02X\n", c); */

    // If the peek is successful,
    // and the next byte is `\n`
    if ((n > 0) && (c == '\n'))
     // Read the next byte
     recv(sock, &c, 1, 0);

    // If the peek is not successful,
    // or the next byte is not `\n`
    else
     // Set the byte be `\n`.
     // This means treat a single `\r` as `\r\n`
     c = '\n';
   }

   // Add the byte to the buffer.
   // Notice `\r\n`, `\r` and '\n' are normalized to `\n` in the buffer.
   buf[i] = c;

   // Increment buffer index
   i++;
  }

  // If the read is not successful
  else
   // Set the byte be `\n`
   c = '\n';
 }

 // Add nul to string end
 buf[i] = '\0';

 // Return the number of bytes read
 return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
 // Buffer
 char buf[1024];

 // Unused
 (void)filename;  /* could use filename to determine file type */

 // Add HTTP start line to the buffer
 strcpy(buf, "HTTP/1.0 200 OK\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add `Server` header to the buffer
 strcpy(buf, SERVER_STRING);

 // Send
 send(client, buf, strlen(buf), 0);

 // Add `Content-Type` header to the buffer
 sprintf(buf, "Content-Type: text/html\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add empty line to the buffer to mean end of headers part
 strcpy(buf, "\r\n");

 // Send
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
 // Buffer
 char buf[1024];

 // Add HTTP start line to the buffer
 sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add `Server` header to the buffer
 sprintf(buf, SERVER_STRING);

 // Send
 send(client, buf, strlen(buf), 0);

 // Add `Content-Type` header to the buffer
 sprintf(buf, "Content-Type: text/html\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add empty line to the buffer to mean end of headers part
 sprintf(buf, "\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "<BODY><P>The server could not fulfill\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "your request because the resource specified\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "is unavailable or nonexistent.\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "</BODY></HTML>\r\n");

 // Send
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
 // File pointer
 FILE *resource = NULL;

 // Number of bytes read
 int numchars = 1;

 // Buffer
 char buf[1024];

 // Set the buffer's initial value be "A\0"
 buf[0] = 'A'; buf[1] = '\0';

 // While the last read is successful,
 // and the buffer's first byte is not `\n`,
 // it means the input data's headers part is not run out.
 while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
  // Read one header line
  numchars = get_line(client, buf, sizeof(buf));

 // Open the file
 resource = fopen(filename, "r");

 // If opening file is not successful
 if (resource == NULL)
  // Send 404 response
  not_found(client);

 // If opening file is successful
 else
 {
  // Send headers
  headers(client, filename);

  // Send file data
  cat(client, resource);
 }

 // Close the file
 fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
 // Socket FD
 int httpd = 0;

 // Address structure
 struct sockaddr_in name;

 // Create server socket, get socket FD
 httpd = socket(PF_INET, SOCK_STREAM, 0);

 // If have error
 if (httpd == -1)
  // Raise error
  error_die("socket");

 // If not have error.

 // Clear the address structure
 memset(&name, 0, sizeof(name));

 // Set address family
 name.sin_family = AF_INET;

 // Set source port
 name.sin_port = htons(*port);

 // Set source IP
 name.sin_addr.s_addr = htonl(INADDR_ANY);

 // Bind the socket.
 //
 // If have error.
 if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
  // Raise error
  error_die("bind");

 // If not have error.

 // If use random port
 if (*port == 0)  /* if dynamically allocating a port */
 {
  // Get the address structure's size
  socklen_t namelen = sizeof(name);

  // Query the bound address.
  //
  // If have error.
  if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
   // Raise error
   error_die("getsockname");

  // If not have error.

  // Set the port variable be the allocated port
  *port = ntohs(name.sin_port);
 }

 // Start listening.
 //
 // If have error.
 if (listen(httpd, 5) < 0)
  // Raise error
  error_die("listen");

 // If not have error.

 // Return the server socket FD
 return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
 // Buffer
 char buf[1024];

 // Add HTTP start line to the buffer
 sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add `Server` header to the buffer
 sprintf(buf, SERVER_STRING);

 // Send
 send(client, buf, strlen(buf), 0);

 // Add `Content-Type` header to the buffer
 sprintf(buf, "Content-Type: text/html\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add empty line to the buffer to mean end of headers part
 sprintf(buf, "\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "</TITLE></HEAD>\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");

 // Send
 send(client, buf, strlen(buf), 0);

 // Add HTML page chunk to the buffer
 sprintf(buf, "</BODY></HTML>\r\n");

 // Send
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

/**
 * Main function.
 */
int main(void)
{
 // Server socket FD
 int server_sock = -1;

 // Port number. 0 means use random port.
 u_short port = 0;

 // Client socket FD
 int client_sock = -1;

 // Client address structure
 struct sockaddr_in client_name;

 // Client address structure's size
 socklen_t client_name_len = sizeof(client_name);

 /* Commented for compiling in Linux.
 // Thread pointer
 pthread_t newthread;
  */

 // Create server socket
 server_sock = startup(&port);

 // Print message
 printf("httpd running on port %d\n", port);

 // Loop forever
 while (1)
 {
  // Accept a request
  client_sock = accept(server_sock,
                       (struct sockaddr *)&client_name,
                       &client_name_len);

  // If have error
  if (client_sock == -1)
   // Raise error
   error_die("accept");

  // If not have error.

  // Process the request
  accept_request(client_sock);  // Uncommented for compiling in Linux

  /* Commented for compiling in Linux.
  // Create thread.
  //
  // If have error.
  if (pthread_create(&newthread , NULL, accept_request, client_sock) != 0)
   // Raise error
   perror("pthread_create");

  // If not have error.
   */
 }

 // Close the server socket
 close(server_sock);

 // Exit without error
 return(0);
}
