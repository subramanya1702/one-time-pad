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
  fprintf(stderr, "ENC_CLIENT: %s\n", msg);
  exit(1); 
}

/*
  Function to receive cipher text from the enc_server
*/
void receiveCipherText(int socketFD, long cipherLength) {
  char *cipherBuffer = malloc(cipherLength + (cipherLength / 1000));
  char tempBuffer[1001];
  int charsRead;
  long index = 0;

  memset(cipherBuffer, '\0', cipherLength);

  while (index < cipherLength) {
    memset(tempBuffer, '\0', 1001);
    charsRead = recv(socketFD, tempBuffer, 1001, 0);

    if (charsRead < 0) {
      error("ERROR reading from the socket");
    }

    strcat(cipherBuffer, tempBuffer);
    index += 1000;
  }

  int charsWritten = send(socketFD, "Received Cipher", 16, 0);
  if (charsWritten < 0) {
    error("ERROR reading from the socket");
  }

  fprintf(stdout, "%s\n", cipherBuffer);
  memset(cipherBuffer, '\0', cipherLength);
  memset(tempBuffer, '\0', 1001);
}


/*
  Function to copy 1000 characters from cipher to a buffer
*/
void copyContentToTempBuffer(char tempBuffer[], char *content, int index) {
  for (int i = 0; i < 1000; i++) {
    tempBuffer[i] = content[index + i];
  }
  tempBuffer[1000] = '\0';
}

/*
  Function to send content to enc_server
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
  Function to send the content length to enc_server
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
  It writes an error to stderr if the received message is not ENC_SERVER.
*/
void tcpHandshake(int socketFD) {
  char *handshakeMessage = "ENC_CLIENT";
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

  if (strcmp(handshakeBuffer, "ENC_SERVER") != 0) {
    error("Handshake failed. Unable to connect to the server");
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
    fprintf(stderr, "ENC_CLIENT: ERROR, no such host\n"); 
    exit(0); 
  }

  memcpy((char*) &address->sin_addr.s_addr, 
        hostInfo->h_addr_list[0],
        hostInfo->h_length);
}

/*
  Function to validate the contents of plain text and key file.
  Checks if the contents contain any character other than the supported set of characters.
  If found, throw an error to stderr and exit out of the program.
*/
void validateInput(char *plainText, char *keyFile) {
  size_t plainTextLength = strlen(plainText);
  size_t keyLength = strlen(keyFile);

  if (plainTextLength > keyLength) {
    error("ERROR: KeyFile is shorter than the PlainText");
  }

  for (int i = 0; i < keyLength; i++) {
    if (i < plainTextLength) {
      if ((plainText[i] >= 'A' && plainText[i] <= 'Z') || plainText[i] == ' ') {
        continue;
      } else {
        fprintf(stderr, "ERROR: The character: %c is not supported in the given file\n", plainText[i]);
        exit(1);
      }
    }

    if ((keyFile[i] >= 'A' && keyFile[i] <= 'Z') || keyFile[i] == ' ') {
      continue;
    } else {
        fprintf(stderr, "ERROR: The character: %c is not supported in the given file\n", keyFile[i]);
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
    fprintf(stderr,"USAGE: %s plaintext keyfile port\n", argv[0]); 
    exit(0); 
  }

  // Open the files
  FILE *pTextFile;
  if ((pTextFile = fopen(argv[1], "r")) == NULL) {
    error("ERROR: Unable to open plaintext file");
  }

  FILE *kFile;
  if ((kFile = fopen(argv[2], "r")) == NULL) {
    error("ERROR: Unable to open key file");
  }

  // Read the content of the files
  char *pContent = readFile(pTextFile);
  char *kContent = readFile(kFile);

  // Validate the inputs
  validateInput(pContent, kContent);

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

  // Send plaintext
  size_t pContentLength = strlen(pContent);
  sendContentLength(pContentLength, socketFD);
  sendContent(pContent, socketFD, pContentLength);

  // Send key
  size_t kContentLength = strlen(kContent);
  sendContentLength(kContentLength, socketFD);
  sendContent(kContent, socketFD, kContentLength);

  // Receive ciphertext
  receiveCipherText(socketFD, (long) strlen(pContent));

  // Close the socket
  close(socketFD); 
  return 0;
}