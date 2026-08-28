#include "crypto.h"
#include <string.h>

void applyXOR(char* buffer, int length, const char* key) {
    int keyLen = strlen(key);
    
    //empty key, avoid divide by zeo
    if (keyLen == 0) return; 

    //loop through every byte in the buffer
    for (int i = 0; i < length; i++) {
        //buffer byte (XOR) corresponding key byte
        buffer[i] ^= key[i % keyLen]; //modulo loops key index
    }
}