#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>

typedef struct SymbolCode {
    int codeLen;
    uint32_t* code;
} SymbolCode;

void errExit(char* msg, int printErr) {
    if(printErr == 0) {
        fprintf(stderr, "%s\n", msg);
    } else {
        perror(msg);
    }
    exit(EXIT_FAILURE);
}

void printCode(SymbolCode sc) {
    int numElems = ceil((double) sc.codeLen / 32);
    for(int i = 0; i < numElems; i++) {
        int numBits = 32;
        if(sc.codeLen % 32 != 0 && i == numElems - 1)
            numBits = sc.codeLen % 32;
        uint32_t mask = 1 << (numBits - 1);
        while(mask != 0) {
            if(sc.code[i] & mask)
                printf("1");
            else
                printf("0");
            mask >>= 1;
        }
    }
    printf("\n");
}

// --------------------------------------------------------------------------
// HUFFMAN TREE OPERATIONS

typedef struct HuffmanNode {
    uint8_t c;
    struct HuffmanNode* left;
    struct HuffmanNode* right;
} HuffmanNode;

HuffmanNode* newNode() {
    HuffmanNode* n = malloc(sizeof(HuffmanNode));
    n -> c = 0;
    n -> left = NULL;
    n -> right = NULL;
    return n;
}

void addCodeToTree(HuffmanNode* root, SymbolCode sc, uint8_t c) {
    int numElems = ceil((double) sc.codeLen / 32);
    for(int i = 0; i < numElems; i++) {
        int numBits = 32;
        if(sc.codeLen % 32 != 0 && i == numElems - 1)
            numBits = sc.codeLen % 32;
        uint32_t mask = 1 << (numBits - 1);
        while(mask != 0) {
            if(sc.code[i] & mask) {
                if(root -> right == NULL) 
                    root -> right = newNode();
                root = root -> right;
            } else {
                if(root -> left == NULL)
                    root -> left = newNode();
                root = root -> left;
            }
            mask >>= 1;
        }
    }
    root -> c = c;
}

HuffmanNode* reconstructHuffmanTree(SymbolCode codeTable[256]) {
    HuffmanNode* root = newNode();
    for(int i = 0; i < 256; i++) {
        if(codeTable[i].codeLen > 0) {
            addCodeToTree(root, codeTable[i], i);
        }
    }
    return root;
}

// --------------------------------------------------------------------------
// FILE DECODING 

uint32_t readBitsFromBuffer(uint32_t* buffer, int* offset, int length) {
    int arrayOffset = *offset / 32;
    int extraOffset = *offset % 32;
    uint32_t mask = 1 << (31 - extraOffset);
    uint32_t res = 0;
    for(int i = 0; i < length; i++) {
        if(buffer[arrayOffset] & mask)
            res = 2 * res + 1;
        else
            res = 2 * res;
        mask >>= 1;
        if(mask == 0) {
            arrayOffset++;
            mask = 1 << 31;
        }
    }
    *offset += length;
    return res;
}

void decodeHeader(uint32_t* fin, int*offset, SymbolCode codeTable[256]) {
    *offset = 0;
    uint32_t charCount = readBitsFromBuffer(fin, offset, 8);
    // printf("%u chars\n", charCount);
    for(int i = 0; i < charCount; i++) {
        uint32_t c = readBitsFromBuffer(fin, offset, 8);
        codeTable[c].codeLen = (int) readBitsFromBuffer(fin, offset, 8);
        int numElems = ceil((double) codeTable[c].codeLen / 32);
        codeTable[c].code = malloc(sizeof(uint32_t) * numElems);
        for(int i = 0; i < numElems; i++) {
            int numBits = 32;
            if(codeTable[c].codeLen % 32 != 0 && i == numElems - 1)
                numBits = codeTable[c].codeLen % 32;
            codeTable[c].code[i] = readBitsFromBuffer(fin, offset, numBits);
        }
    }
}

uint32_t readCharCodeFromBuffer(uint32_t* buffer, int* offset, HuffmanNode* root) {
    int arrayOffset = *offset / 32;
    int extraOffset = *offset % 32;
    uint32_t mask = 1 << (31 - extraOffset);
    uint8_t cres = 0;
    int length = 0;
    HuffmanNode* node = root;
    while(1) {
        if(buffer[arrayOffset] & mask)
            node = node -> right;
        else
            node = node -> left;
        length++;
        // check if valid code
        if(node == NULL) {
            errExit("decode -> no match found", 0);
        }
        if(node -> left == NULL && node -> right == NULL) {
            cres = node->c;
            break;
        }
        mask >>= 1;
        if(mask == 0) {
            arrayOffset++;
            mask = 1 << 31;
        }
    }
    *offset += length;
    return cres;
}

void decodeFile(uint8_t* fout, int* offset, uint32_t* fin, int outputFileSize, HuffmanNode* root) {
    for(int i = 0; i < outputFileSize; i++) {
        fout[i] = readCharCodeFromBuffer(fin, offset, root);
        // printf("[%d / %d]: %c (%u), %d\n", i+1, outputFileSize, fout[i], fout[i], *offset);
    }
}

// --------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // parse command line args
    if(argc != 3 || (argc > 1 && strcmp(argv[1], "--help") == 0)) {
        fprintf(stderr, "Usage: %s <encoded text file> <output file name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char* filename = argv[1];
    char* outfilename = argv[2];

    double start, end;
    struct timeval timecheck;

    gettimeofday(&timecheck, NULL);
    start = timecheck.tv_sec + (double)timecheck.tv_usec / 1000000;

    // map input and output files
    int in_fd = open(filename, O_RDONLY);
    int inputFileSize = lseek(in_fd, 0, SEEK_END);
    // printf("INPUT SIZE: %d\n", inputFileSize);
    lseek(in_fd, 0, SEEK_SET);
    uint32_t* fin = (uint32_t*) mmap(NULL, inputFileSize, PROT_READ, MAP_PRIVATE, in_fd, 0);

    int outputFileSize = *fin;
    // printf("OUTPUT SIZE: %d\n", outputFileSize);
    int out_perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    int out_fd = open(outfilename, O_CREAT | O_RDWR, out_perm);
    ftruncate(out_fd, outputFileSize);
    uint8_t* fout = (uint8_t*) mmap(NULL, outputFileSize, PROT_READ | PROT_WRITE, MAP_SHARED, out_fd, 0);
    memset(fout, 0, outputFileSize);
    fin++;

    // decode header
    SymbolCode codeTable[256];
    for(int i = 0; i < 256; i++)
        codeTable[i].codeLen = -1;
    int bitOffset = 0;
    decodeHeader(fin, &bitOffset, codeTable);

    // ------- DEBUG
    // for(int i = 0; i < 256; i++)
    //     if(codeTable[i].codeLen > 0) {
    //         printf("%c: ", i);
    //         printCode(codeTable[i]);
    //     }

    // reconstruct huffman tree
    HuffmanNode* root = reconstructHuffmanTree(codeTable);

    // decode file
    decodeFile(fout, &bitOffset, fin, outputFileSize, root);

    // close file mappings
    munmap(fin, inputFileSize);
    munmap(fout, outputFileSize);
    close(in_fd);
    close(out_fd);

    gettimeofday(&timecheck, NULL);
    end = timecheck.tv_sec + (double) timecheck.tv_usec / 1000000;

    printf("%lf seconds elapsed\n", end - start);
}