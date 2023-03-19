#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main (int argc, char *argv[]) {

    // If the number of arguments is not 2 then write an error to stderr
    if (argc != 2) {
        fprintf(stderr, "Error: Invalid number of arguments\n");
        return 1;
    }
    
    // Convert the key length argument to an integer
    int keyLength = atoi(argv[1]);

    // Initialize a character array of size key length + 2 (A newline character and a NULL)
    char keyFile[keyLength + 2];
    int r;

    // Seed the random number generator with the current time, so that the key file is unique every time it is generated
    srand(time(0));

    for (int j = 0; j < keyLength; j++) {
        r = (rand() % 27) + 1;

        // If the random number is 27, replace it with the space character. 
        // Or else, replace it with upper case alphabets with ASCII values ranging from 65 to 90 (64 + r)
        if (r == 27) {
            keyFile[j] = 32;
        } else {
            keyFile[j] = 64 + r;
        }
    }

    // Replace the penultimate character with a newline and the last character with a NULL terminator
    keyFile[keyLength] = '\n';
    keyFile[keyLength + 1] = '\0';

    // Write the key to stdout
    fprintf(stdout, "%s", keyFile);
    
    return 0;
}