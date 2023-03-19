#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netdb.h>

/*
  Function to write messages/errors to stderr
*/
void error(const char *msg) { 
  fprintf(stderr, "DEC_CLIENT: %s\n", msg); 
  exit(1); 
}

/*
  Function to receive plain text from the dec_server
*/
void receivePlainText(int socketFD, long plainTextLength) {
  char *plainTextBuffer = malloc(plainTextLength + (plainTextLength / 1000));
  char tempBuffer[1001];
  int charsRead;
  long index = 0;

  memset(plainTextBuffer, '\0', plainTextLength);

  while (index < plainTextLength) {
    memset(tempBuffer, '\0', 1001);
    charsRead = recv(socketFD, tempBuffer, 1001, 0);

    if (charsRead < 0) {
      error("ERROR reading from the socket");
    }

    strcat(plainTextBuffer, tempBuffer);
    index += 1000;
  }

  int charsWritten = send(socketFD, "Received PlainText", 19, 0);
  if (charsWritten < 0) {
    error("ERROR reading from the socket");
  }

  fprintf(stdout, "%s\n", plainTextBuffer);
  memset(plainTextBuffer, '\0', plainTextLength);
  memset(tempBuffer, '\0', 1001);
}

/*
  Function to copy 1000 characters from plain text to a buffer
*/
void copyContentToTempBuffer(char tempBuffer[], char *content, int index) {
  for (int i = 0; i < 1000; i++) {
    tempBuffer[i] = content[index + i];
  }
  tempBuffer[1000] = '\0';
}

/*
  Function to send content to dec_server
*/
void sendContent(char *content, int socketFD, size_t contentLength) {
  char tempBuffer[1001];
  int index = 0, charsWritten;

  while (index < contentLength) {
    memset(tempBuffer, '\0', 1001);
    copyContentToTempBuffer(tempBuffer, content, index);

    charsWritten = send(socketFD, tempBuffer, 1001, 0);
    if (charsWritten < 0){
      error("ERROR writing to socket");
    }
    index += 1000;
  }
  memset(tempBuffer, '\0', 1001);

  char buffer[9];
  memset(buffer, '\0', 9);
  int charsRead = recv(socketFD, buffer, 9, 0); 
  if (charsRead < 0){
    error("ERROR reading from socket");
  }

  memset(buffer, '\0', 9);
}

/*
  Function to send the content length to dec_server
*/
void sendContentLength(size_t contentLength, int socketFD) {
  char cLength[11];
  sprintf(cLength, "%lu", contentLength);

  int charsWritten = send(socketFD, cLength, strlen(cLength), 0);
  if (charsWritten < 0) {
    error("ERROR writing to socket");
  }

  char buffer[9];
  memset(buffer, '\0', 9);
  int charsRead = recv(socketFD, buffer, 9, 0); 
  if (charsRead < 0){
    error("ERROR reading from socket");
  }

  memset(buffer, '\0', 9);
}

/*
  Function to perform (psuedo) a 2 step TCP Handshake with the server program(s).
  It writes an error to stderr if the received message is not DEC_SERVER.
*/
void tcpHandshake(int socketFD) {
  char *handshakeMessage = "DEC_CLIENT";
  int hCharsWritten = send(socketFD, handshakeMessage, strlen(handshakeMessage), 0);
  if (hCharsWritten < 0) {
    error("ERROR writing to socket");
  }

  char handshakeBuffer[11];
  memset(handshakeBuffer, '\0', sizeof(handshakeBuffer));
  int hCharsRead = recv(socketFD, handshakeBuffer, sizeof(handshakeBuffer) - 1, 0); 
  if (hCharsRead < 0){
    error("ERROR reading from socket");
  }
  
  if (strcmp(handshakeBuffer, "DEC_SERVER") != 0) {
    error("Handshake failed. Cannot connect to the server");
  }

  memset(handshakeBuffer, '\0', sizeof(handshakeBuffer));
}

/*
  Function to set up the server address struct
*/
void setupAddressStruct(struct sockaddr_in* address, int portNumber){
  memset((char*) address, '\0', sizeof(*address)); 

  address->sin_family = AF_INET;
  address->sin_port = htons(portNumber);

  struct hostent* hostInfo = gethostbyname("localhost");

  if (hostInfo == NULL) { 
    fprintf(stderr, "DEC_CLIENT: ERROR, no such host\n"); 
    exit(0); 
  }

  memcpy((char*) &address->sin_addr.s_addr, 
        hostInfo->h_addr_list[0],
        hostInfo->h_length);
}

/*
  Function to validate the contents of cipher text and key file.
  Checks if the contents contain any character other than the supported set of characters.
  If found, throw an error to stderr and exit out of the program.
*/
void validateInput(char *cipher, char *key) {
  size_t cipherLength = strlen(cipher);
  size_t keyLength = strlen(key);

  if (cipherLength > keyLength) {
    error("ERROR: KeyFile is shorter thanb CipherText");
  }

  for (int i = 0; i < keyLength; i++) {
    if (i < cipherLength) {
      if ((cipher[i] >= 'A' && cipher[i] <= 'Z') || cipher[i] == ' ') {
        continue;
      } else {
        fprintf(stderr, "ERROR: The character: %c is not supported in the given file\n", cipher[i]);
        exit(1);
      }
    }

    if ((key[i] >= 'A' && key[i] <= 'Z') || key[i] == ' ') {
      continue;
    } else {
        fprintf(stderr, "ERROR: The character: %c is not supported in the given file\n", key[i]);
        exit(1);
    }
  }
}

/*
  Function to read the contents of a file
*/
char* readFile(FILE *fp) {
  // Sets the file position pointer to EOF
  fseek(fp, 0, SEEK_END);
  // Get the position of the file pointer
  long fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *buffer = malloc(fileSize);
  if (buffer == NULL) {
    error("ERROR: Unable to allocate memory for buffer while reading the file");
  }

  fread(buffer, fileSize, 1, fp);
  fclose(fp);

  if (buffer[strlen(buffer) - 1] == '\n') {
    buffer[strlen(buffer) - 1] = '\0';
  }

  return buffer;
}

/*
  Main Function -> Entrypoint
*/
int main(int argc, char *argv[]) {
  // Variable definitions
  int socketFD, portNumber;
  struct sockaddr_in serverAddress;

  // Check if the required number of argumemnts are passed
  if (argc < 4) { 
    fprintf(stderr,"USAGE: %s ciphertext keyfile port\n", argv[0]); 
    exit(0); 
  }

  // Open the files
  FILE *cipherTextFile;
  if ((cipherTextFile = fopen(argv[1], "r")) == NULL) {
    error("ERROR: Unable to open ciphertext file");
  }

  FILE *keyFile;
  if ((keyFile = fopen(argv[2], "r")) == NULL) {
    error("ERROR: Unable to open key file");
  }

  // Read the content of the files
  char *cipherContent = readFile(cipherTextFile);
  char *keyContent = readFile(keyFile);

  // Validate the inputs
  validateInput(cipherContent, keyContent);

  // Create a socket
  if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    error("ERROR opening socket");
  }

  // Set up the server address struct
  setupAddressStruct(&serverAddress, atoi(argv[3]));

  // Connect the socketFD to the server address
  if (connect(socketFD, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0){
    error("ERROR connecting");
  }

  // Commence TCP Handshake
  tcpHandshake(socketFD);

  // Send ciphertext
  size_t cipherTextLength = strlen(cipherContent);
  sendContentLength(cipherTextLength, socketFD);
  sendContent(cipherContent, socketFD, cipherTextLength);

  // Send key
  size_t keyLength = strlen(keyContent);
  sendContentLength(keyLength, socketFD);
  sendContent(keyContent, socketFD, keyLength);

  // Receive plaintext
  receivePlainText(socketFD, (long) strlen(cipherContent));

  // Close the socket
  close(socketFD); 
  return 0;
}