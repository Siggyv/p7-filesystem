#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 512

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <output file> <n>\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    int n = atoi(argv[2]);

    if (n <= 0) {
        fprintf(stderr, "Error: 'n' must be a positive integer\n");
        return 1;
    }

    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        perror("Failed to open file");
        return 1;
    }

    // Create a buffer to fill with data
    char buffer[BLOCK_SIZE];
    memset(buffer, 'A', BLOCK_SIZE);  // Fill buffer with character 'A'

    // Write the buffer to file n times
    for (int i = 0; i < n; i++) {
        if (fwrite(buffer, sizeof(char), BLOCK_SIZE, fp) != BLOCK_SIZE) {
            perror("Failed to write to file");
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    printf("File created successfully: %s\n", filename);
    return 0;
}

