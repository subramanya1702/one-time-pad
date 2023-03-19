#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

static const char upperAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

/*
  Function to write messages/errors to stderr
*/
void error(const char *msg) {
  fprintf(stderr, "DEC_SERVER: %s\n", msg);
  exit(1);
}

/*
  Function to copy 1000 characters from plain text to a buffer
*/
void copyContentToTempBuffer(char tempBuffer[], char *plainText, int index) {
  for (int i = 0; i < 1000; i++) {
    tempBuffer[i] = plainText[index + i];
  }
  tempBuffer[1000] = '\0';
}

/*
  Function to send the decoded plain text to the client.
  It sends the plain text as chunks of 1000 characters.
*/
void sendPlainText(int connectionSocket, char *plainText) {
  char tempBuffer[1001];
  int index = 0, charsWritten;
  size_t plainTextLength = strlen(plainText);

  while (index < plainTextLength) {
    memset(tempBuffer, '\0', 1001);
    copyContentToTempBuffer(tempBuffer, plainText, index);

    charsWritten = send(connectionSocket, tempBuffer, 1001, 0);
    if (charsWritten < 0) {
      error("ERROR writing to the socket");
    }
    index += 1000;
  }

  memset(tempBuffer, '\0', 1001);

  char plainTextReceiveBuffer[19];
  memset(plainTextReceiveBuffer, '\0', 19);

  int charsRead = recv(connectionSocket, plainTextReceiveBuffer, 19, 0);
  if (charsRead < 0) {
    error("ERROR reading from the socket");
  }
}

/*
  Function to decode the cipher to plain text
*/
void decodeCipher(char cipherText[], int cipherTextLength, char keyFile[], char plainText[]) {
    for (int i = 0; i < cipherTextLength; i++) {
        int cpValue;
        int kfValue;

        if (cipherText[i] == ' ') {
            cpValue = 26;
        } else {
            cpValue = ((int) cipherText[i] - 65);
        }

        if (keyFile[i] == ' ') {
            kfValue = 26;
        } else {
            kfValue = ((int) keyFile[i] - 65);
        }

        if (cpValue - kfValue < 0) {
            plainText[i] = upperAlphabet[27 + (cpValue - kfValue)];
        } else {
            plainText[i] = upperAlphabet[(cpValue - kfValue) % 27];
        }
    }
    
    plainText[cipherTextLength] = '\0';
}

/*
  Function to get the plain text/key from the dec_client
*/
void getContent(int contentLength, int connectionSocket, char contentBuffer[]) {
  char tempBuffer[1001];
  int index, charsRead;

  memset(contentBuffer, '\0', contentLength);

  while (index < contentLength) {
    memset(tempBuffer, '\0', 1001);
    charsRead = recv(connectionSocket, tempBuffer, 1001, 0);

    if (charsRead < 0){
      error("ERROR reading from socket");
    }
    
    strcat(contentBuffer, tempBuffer);
    index += 1000;
  }
  
  memset(tempBuffer, '\0', 1001);

  charsRead = send(connectionSocket, "RECEIVED", 9, 0);
  if (charsRead < 0){ 
    error("ERROR writing to socket");
  }
}

/*
  Function to get the plain text/key length from the dec_client
*/
long getContentLength(int connectionSocket) {
  char lenBuffer[11];
  memset(lenBuffer, '\0', 11);

  int charsRead = recv(connectionSocket, lenBuffer, 10, 0);
  if (charsRead < 0) {
    error("ERROR reading from the socket");
  }

  long contentLength = atoi(lenBuffer);
  memset(lenBuffer, '\0', 11);

  charsRead = send(connectionSocket, "RECEIVED", 9, 0);
  if (charsRead < 0) {
    error("ERROR writing to the socket");
  }

  return contentLength;
}

/*
  Function to perform (psuedo) a 2 step TCP Handshake with the client program(s).
  It writes an error to stderr and sends an ERROR message to the client
  if the received message is not DEC_CLIENT
*/
int tcpHandshake(int connectionSocket) {
  // A flag to notify the caller whether to throw an error
  int tcpHandshakeFailed = 0;
  char handshakeBuffer[11];

  memset(handshakeBuffer, '\0', sizeof(handshakeBuffer));

  // Recive the handshake message from the client
  int hCharsRead = recv(connectionSocket, handshakeBuffer, sizeof(handshakeBuffer) - 1, 0); 
  if (hCharsRead < 0){
    error("ERROR reading from socket");
  }
  
  char *handshakeMessage = NULL;

  // Check if the received message is DEC_CLIENT
  if (strcmp(handshakeBuffer, "DEC_CLIENT") != 0) {
    tcpHandshakeFailed = 1;
    handshakeMessage = "ERROR";
  } else {
    handshakeMessage = "DEC_SERVER";
  }
  memset(handshakeBuffer, '\0', sizeof(handshakeBuffer));

  // Send an acknowledgement back to the client to complete the handshake
  int hCharsWritten = send(connectionSocket, handshakeMessage, strlen(handshakeMessage), 0);
  if (hCharsWritten < 0) {
    error("ERROR writing to socket");
  }

  return tcpHandshakeFailed;
}

/*
  Function to handle SIGCHLD signal
*/
void sigchld_handler(int s) {
  int saved_errno = errno;

  while(waitpid(-1, NULL, WNOHANG) > 0);

  errno = saved_errno;
}

/*
  Function to set up the server address struct
*/
void setupAddressStruct(struct sockaddr_in* address, int portNumber){
  memset((char*) address, '\0', sizeof(*address)); 
  address->sin_family = AF_INET;
  address->sin_port = htons(portNumber);
  address->sin_addr.s_addr = INADDR_ANY;
}

/*
  Main Function -> Entrypoint
*/
int main(int argc, char *argv[]){
  // Variable definitions
  int connectionSocket;
  struct sockaddr_in serverAddress, clientAddress;
  socklen_t sizeOfClientInfo = sizeof(clientAddress);
  struct sigaction sigact;
  
  // Check if the required number of argumemnts are passed
  if (argc < 2) { 
    fprintf(stderr,"USAGE: %s port\n", argv[0]); 
    exit(1);
  } 
  
  // Create a socket
  int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocket < 0) {
    error("ERROR opening socket");
  }

  // Set up the server address struct
  setupAddressStruct(&serverAddress, atoi(argv[1]));

  // Bind the port to the server address
  if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0){
    error("ERROR on binding");
  }

  // Mark the socket as a passive socket for incoming requests
  listen(listenSocket, 5);

  // Signal handler to reap all the zombie processes
  // Credits: Beej's networking guide
  sigact.sa_handler = sigchld_handler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sigact, NULL) == -1) {
      error("sigaction()");
  }
  
  while(1){
    // Accept the pending connections from the connection queue
    connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); 

    if (connectionSocket < 0){
      error("ERROR on accept");
    }
    
    // Create a new process
    pid_t spawnpid = fork();

    if (spawnpid == -1) {
        error("fork() failed!");
        exit(1);
    } else if (spawnpid == 0) {
      // Close the listen socket as it is not needed for the child
      close(listenSocket);

      if (tcpHandshake(connectionSocket)) {
        close(connectionSocket);
        error("Handshake failed. Unable to connect to the client");
      }

      // Receive cipher text
      int cipherTextLength = getContentLength(connectionSocket);
      char cipherTextBuffer[cipherTextLength + (cipherTextLength / 1000)];
      getContent(cipherTextLength, connectionSocket, cipherTextBuffer);

      // Receive key
      int keyLength = getContentLength(connectionSocket);
      char keyBuffer[keyLength + (keyLength / 1000)];
      getContent(keyLength, connectionSocket, keyBuffer);

      // Generate plain text and send it to the client
      char plainText[cipherTextLength + 1];
      decodeCipher(cipherTextBuffer, cipherTextLength, keyBuffer, plainText);
      sendPlainText(connectionSocket, plainText);

      // Close the connection socket
      close(connectionSocket); 
      exit(0);
    }

    // Close the connection socket for the parent as it is not needed
    close(connectionSocket);
  }

  // Close the listen socket
  close(listenSocket); 
  return 0;
}