//Generates an AES Key
//Thomas Salemy

#include <iostream>
#include <stdio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/aes.h>

//size of AES key
const int AES_256_KEY_SIZE = 32;

//size of blocks that get encrypted
const int BLOCK_SIZE = 16;

//chunk size for reading/writing from files
const int BUFSIZE = 1024;

void generate_aes_key_file(std::string keyfile) {
  // Generate an encryption key and initialization vector for AES, using a
  // cryptographically sound algorithm
  unsigned char key[AES_256_KEY_SIZE], iv[BLOCK_SIZE];
  if (!RAND_bytes(key, sizeof(key)) || !RAND_bytes(iv, sizeof(iv))) {
    fprintf(stderr, "Error in RAND_bytes()\n");
    exit(0);
  }

  // Create or truncate the key file
  FILE *file = fopen(keyfile.c_str(), "wb");
  if (!file) {
    perror("Error in fopen");
    exit(0);
  }

  // write key and iv to the file, and close it
  fwrite(key, sizeof(unsigned char), sizeof(key), file);
  fwrite(iv, sizeof(unsigned char), sizeof(iv), file);
  fclose(file);
}


int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Provide the filename as second argument\n" << std::endl;
		return 1;
	}

	std::string filename(argv[1]);
	generate_aes_key_file(filename);
}