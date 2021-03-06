#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>

typedef struct SymbolCode {
    int codeLen;
    uint32_t* code;
} SymbolCode;

typedef struct HuffmanNode {
    uint8_t c;
    struct HuffmanNode* left;
    struct HuffmanNode* right;
} HuffmanNode;

void errExit(char* msg, int printErr) {
    if(printErr == 0) {
        fprintf(stderr, "%s\n", msg);
    } else {
        perror(msg);
    }
    exit(EXIT_FAILURE);
}

// --------------------------------------------------------------------------
// GLOBAL VARIABLES

int numThreads;
int* threadIDList;

uint32_t* fin;
uint8_t* fout;
int outputFileSize;

int* inputOffset;

HuffmanNode* root;

// --------------------------------------------------------------------------

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

void decodeHeader(int* offset, SymbolCode codeTable[256]) {
    numThreads = readBitsFromBuffer(fin, offset, 32);
    threadIDList = (int*) malloc(sizeof(int) * numThreads);
    inputOffset = (int*) malloc(sizeof(int) * numThreads);
    for(int i = 0; i < numThreads; i++) {
        threadIDList[i] = i;
        inputOffset[i] = readBitsFromBuffer(fin, offset, 32);
    }
    uint32_t charCount = readBitsFromBuffer(fin, offset, 8);
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

uint32_t readCharCodeFromBuffer(uint32_t* buffer, int* offset) {
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

void* decodeBatch(void* arg) {
    int threadID = *((int*) arg);
    int batchOffset = threadID * (outputFileSize / numThreads);
    int batchSize = (outputFileSize / numThreads);
    if(threadID == numThreads - 1)
        batchSize = outputFileSize - (numThreads - 1) * (outputFileSize / numThreads);

    int offset = inputOffset[threadID];
    for(int i = 0; i < batchSize; i++) {
        fout[batchOffset + i] = readCharCodeFromBuffer(fin, &offset);
    }

    pthread_exit(0);
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
    lseek(in_fd, 0, SEEK_SET);
    fin = (uint32_t*) mmap(NULL, inputFileSize, PROT_READ, MAP_PRIVATE, in_fd, 0);

    int bitOffset = 0;
    outputFileSize = readBitsFromBuffer(fin, &bitOffset, 32);
    int out_perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    int out_fd = open(outfilename, O_CREAT | O_RDWR, out_perm);
    ftruncate(out_fd, outputFileSize);
    fout = (uint8_t*) mmap(NULL, outputFileSize, PROT_READ | PROT_WRITE, MAP_SHARED, out_fd, 0);
    memset(fout, 0, outputFileSize);

    // decode header
    SymbolCode codeTable[256];
    for(int i = 0; i < 256; i++)
        codeTable[i].codeLen = -1;
    decodeHeader(&bitOffset, codeTable);

    // thread variables
    pthread_t threads[numThreads];
    pthread_attr_t threadAttr;
    pthread_attr_init(&threadAttr);

    // reconstruct huffman tree
    root = reconstructHuffmanTree(codeTable);

    // decode file
    for(int i = 0; i < numThreads; i++) {
        pthread_create(&threads[i], &threadAttr, decodeBatch, &threadIDList[i]);
    }
    for(int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    // close file mappings
    munmap(fin, inputFileSize);
    munmap(fout, outputFileSize);
    close(in_fd);
    close(out_fd);

    gettimeofday(&timecheck, NULL);
    end = timecheck.tv_sec + (double) timecheck.tv_usec / 1000000;

    printf("%lf seconds elapsed\n", end - start);
}