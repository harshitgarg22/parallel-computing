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

uint8_t* fin;
uint32_t* fout;
int inputFileSize;

int charFreq[256] = {0};
pthread_mutex_t charFreqMutex;
int** charFreqThreads;

SymbolCode codeTable[256];

int* outputOffset;
int* outputLength;
pthread_mutex_t* outputMutex;
pthread_mutex_t foutMutex;
pthread_cond_t* outputCond;

// --------------------------------------------------------------------------

void* getCharFreq(void* arg) {
    int threadID = *((int*) arg);
    int batchOffset = threadID * (inputFileSize / numThreads);
    int batchSize = (inputFileSize / numThreads);
    if(threadID == numThreads - 1)
        batchSize = inputFileSize - (numThreads - 1) * (inputFileSize / numThreads);

    for(int i = 0; i < batchSize; i++) 
        charFreqThreads[threadID][fin[batchOffset + i]]++;
    
    pthread_mutex_lock(&charFreqMutex);
    for(int i = 0; i < 256; i++)
        charFreq[i] += charFreqThreads[threadID][i];
    pthread_mutex_unlock(&charFreqMutex);

    pthread_exit(0);
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
    int freq;
    struct HuffmanNode* left;
    struct HuffmanNode* right;
} HuffmanNode;

HuffmanNode* newLeafNode(uint8_t c, int freq) {
    HuffmanNode* n = malloc(sizeof(HuffmanNode));
    n -> c = c;
    n -> freq = freq;
    n -> left = NULL;
    n -> right = NULL;
    return n;
}

HuffmanNode* newInternalNode(HuffmanNode* left, HuffmanNode* right) {
    HuffmanNode* n = malloc(sizeof(HuffmanNode));
    n -> c = 0;
    n -> freq = left -> freq + right -> freq;
    n -> left = left;
    n -> right = right;
    return n;
}

// --------------------------------------------------------------------------
// PRIORITY QUEUE OPERATIONS

typedef struct priorityQueue {
    int size;
    int cap;
    HuffmanNode** arr;
} priorityQueue;

int parent(int i) {return (i + 1) / 2 - 1;}
int left(int i) {return 2*i + 1;}
int right(int i) {return 2*i + 2;}

void swapNodes(HuffmanNode** arr, int i, int j) {
    HuffmanNode* temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
}

void minHeapify(priorityQueue q, int i) {
    int l = left(i);
    int r = right(i);
    int smallest = i;
    if(l >= 0 && l < q.size && q.arr[l]->freq < q.arr[smallest]->freq)
        smallest = l;
    if(r >= 0 && r < q.size && q.arr[r]->freq < q.arr[smallest]->freq)
        smallest = r;
    
    if(smallest != i) {
        swapNodes(q.arr, i, smallest);
        minHeapify(q, smallest);
    }
}

void buildMinPQ(priorityQueue q) {
    for(int i = q.size / 2 - 1; i >= 0; i--) {
        minHeapify(q, i);
    }
}

HuffmanNode* getMinPQ(priorityQueue q) {
    return q.arr[0];
}

HuffmanNode* extractMinPQ(priorityQueue* q) {
    HuffmanNode* min = q->arr[0];
    swapNodes(q->arr, 0, q->size - 1);
    q->size = q->size - 1;
    minHeapify(*q, 0);
    return min;
}

int decreaseKeyPQ(priorityQueue q, int i, int key) {
    if(key > q.arr[i]->freq)
        return -1;
    q.arr[i]->freq = key;
    while(i > 0 && q.arr[parent(i)]->freq > q.arr[i]->freq) {
        swapNodes(q.arr, i, parent(i));
        i = parent(i);
    }
    return 0;
}

int insertPQ(priorityQueue* q, HuffmanNode* node) {
    if(q->size == q->cap) {
        q->cap *= 2;
        HuffmanNode** newarr = malloc(q->cap * sizeof(HuffmanNode*));
        for(int i = 0; i < q->size; i++)
            newarr[i] = q->arr[i];
        free(q->arr);
        q->arr=newarr;
    }
    q->size = q->size + 1;
    q->arr[q->size - 1] = node;
    return decreaseKeyPQ(*q, q->size - 1, node->freq);
}

priorityQueue* initializePQ(int charFreq[256]) {
    priorityQueue* q = malloc(sizeof(priorityQueue));
    q->cap = 0;
    for(int i = 0; i < 256; i++) 
        if(charFreq[i] > 0)
            q->cap += 1;
    q->arr = malloc(q->cap * sizeof(HuffmanNode*));
    q->size = q->cap;
    int j = 0;
    for(int i = 0; i < 256; i++) {
        if(charFreq[i] > 0) {
            q->arr[j] = newLeafNode(i, charFreq[i]);
            j++;
        }
    }
    buildMinPQ(*q);
    return q;
}

// --------------------------------------------------------------------------
// HUFFMAN GREEDY ALGORITHM, TRAVERSAL FOR ENCODING SCHEME

HuffmanNode* huffman(int charFreq[256]) {
    priorityQueue* q = initializePQ(charFreq);
    int n = q->size;
    if(n == 1) {
        // edge case, only one kind of char appears in file
        // add a random char to allow huffman tree construction
        char c0 = getMinPQ(*q)->c;
        char c1 = 0;
        if(c0 == c1) 
            c1 = 1;
        HuffmanNode* n1 = newLeafNode(c1, 1);
        insertPQ(q, n1);
        n += 1;
    }
    for(int i = 0; i < n - 1; i++) {
        HuffmanNode* x = extractMinPQ(q);
        HuffmanNode* y = extractMinPQ(q);
        HuffmanNode* z = newInternalNode(x, y);
        insertPQ(q, z);
    }
    return extractMinPQ(q);
}

void getHuffmanCodeLengths(SymbolCode codeTable[256], HuffmanNode* node, int lenval, int* maxlen) {
    if(node -> left == NULL && node -> right == NULL) {
        int numElems = ceil((double) lenval / 32);
        codeTable[node->c].codeLen = lenval;
        codeTable[node->c].code = malloc(sizeof(uint32_t) * numElems);
        if(lenval > *maxlen)
            *maxlen = lenval;
        return;
    }
    if(node -> left) {
        getHuffmanCodeLengths(codeTable, node -> left, lenval + 1, maxlen);
    }
    if(node -> right) {
        getHuffmanCodeLengths(codeTable, node -> right, lenval + 1, maxlen);
    }
}

void getHuffmanCodes(SymbolCode codeTable[256], HuffmanNode* node, uint32_t* codeval, int lenval) {
    int offset = lenval / 32;
    if(node -> left == NULL && node -> right == NULL) {
        int numElems = ceil((double) lenval / 32);
        for(int i = 0; i < numElems; i++) {
            codeTable[node->c].code[i] = codeval[i];
        }
        return;
    }
    if(node -> left) {
        codeval[offset] <<= 1;
        getHuffmanCodes(codeTable, node -> left, codeval, lenval + 1);
        codeval[offset] >>= 1;
    }
    if(node -> right) {
        codeval[offset] = (codeval[offset] << 1) + 1;
        getHuffmanCodes(codeTable, node -> right, codeval, lenval + 1);
        codeval[offset] >>= 1;
    }
}

void populateCodeTable(SymbolCode codeTable[256], HuffmanNode* root) {
    int maxlen = 0;
    getHuffmanCodeLengths(codeTable, root, 0, &maxlen);
    uint32_t* codeval = malloc(sizeof(uint32_t) * (ceil((double) maxlen / 32)));
    getHuffmanCodes(codeTable, root, codeval, 0);
}

// --------------------------------------------------------------------------
// FILE ENCODING

void writeBitsToBuffer(uint32_t* buffer, int* offset, uint32_t code, int codeLen) {
    int arrayOffset = *offset / 32;
    int extraOffset = *offset % 32;
    
    if(extraOffset <= 32 - codeLen) {
        // can be written in full to current block
        uint32_t b = code << (32 - codeLen - extraOffset);
        buffer[arrayOffset] |= b;
    } else {
        // write to two blocks
        uint32_t b1 = code >> (extraOffset - 32 + codeLen);
        buffer[arrayOffset] |= b1;

        int codeLen2 = codeLen - (32 - extraOffset);
        uint32_t b2 = code & ((1 << codeLen2) - 1); // mask bits written in b1
        b2 <<= (32 - codeLen2); // shift to left boundary
        buffer[arrayOffset + 1] |= b2;
    }

    *offset += codeLen;
}

void writeCodeToBuffer(uint32_t* fout, int* offset, SymbolCode sc, int lockBuffer) {
    int numElems = ceil((double) sc.codeLen / 32);
    for(int i = 0; i < numElems; i++) {
        int numBits = 32;
        if(sc.codeLen % 32 != 0 && i == numElems - 1)
            numBits = sc.codeLen % 32;
        if(lockBuffer)
            pthread_mutex_lock(&foutMutex);
        writeBitsToBuffer(fout, offset, sc.code[i], numBits);
        if(lockBuffer)
            pthread_mutex_unlock(&foutMutex);
    }
}

void encodeHeader(uint32_t* fout, int* bitOffset, int inputFileSize, SymbolCode codeTable[256]) {
    *bitOffset = 0;
    writeBitsToBuffer(fout, bitOffset, (uint32_t) inputFileSize, 32);

    writeBitsToBuffer(fout, bitOffset, (uint32_t) numThreads, 32);
    for(int i = 0; i < numThreads; i++) {
        writeBitsToBuffer(fout, bitOffset, (uint32_t) 0, 32);
    }

    uint32_t charCount = 0;
    for(int i = 0; i < 256; i++)
        charCount += (codeTable[i].codeLen > 0);
    writeBitsToBuffer(fout, bitOffset, charCount, 8);
    for(int i = 0; i < 256; i++) {
        if(codeTable[i].codeLen > 0) {
            writeBitsToBuffer(fout, bitOffset, (uint32_t) i, 8);
            writeBitsToBuffer(fout, bitOffset, (uint32_t) codeTable[i].codeLen, 8);
            writeCodeToBuffer(fout, bitOffset, codeTable[i], 0);
        }
    }
}

void* encodeBatch(void* arg) {
    int threadID = *((int*) arg);
    int batchOffset = threadID * (inputFileSize / numThreads);
    int batchSize = (inputFileSize / numThreads);
    if(threadID == numThreads - 1)
        batchSize = inputFileSize - (numThreads - 1) * (inputFileSize / numThreads);
    
    // calculate outputLength
    int myOutputLength = 0;
    for(int i = 0; i < 256; i++) {
        myOutputLength += charFreqThreads[threadID][i] * codeTable[i].codeLen;
    }
    free(charFreqThreads[threadID]);

    pthread_mutex_lock(&outputMutex[threadID]);
    outputLength[threadID] = myOutputLength;
    pthread_mutex_unlock(&outputMutex[threadID]);

    // calculate outputOffset, signal next thread
    int myOutputOffset = 0;
    if(threadID != 0) {
        pthread_mutex_lock(&outputMutex[threadID - 1]);
        while(outputOffset[threadID - 1] == -1 || outputLength[threadID - 1] == -1) {
            pthread_cond_wait(&outputCond[threadID - 1], &outputMutex[threadID - 1]);
        }
        myOutputOffset = outputOffset[threadID - 1] + outputLength[threadID - 1];
        pthread_mutex_unlock(&outputMutex[threadID - 1]);

        pthread_mutex_lock(&outputMutex[threadID]);
        outputOffset[threadID] = myOutputOffset;
        pthread_mutex_unlock(&outputMutex[threadID]);
    } else {
        pthread_mutex_lock(&outputMutex[threadID]);
        myOutputOffset = outputOffset[threadID];
        pthread_mutex_unlock(&outputMutex[threadID]);
    }

    if(threadID != numThreads - 1) {
        pthread_mutex_lock(&outputMutex[threadID]);
        pthread_cond_signal(&outputCond[threadID]);
        pthread_mutex_unlock(&outputMutex[threadID]);
    }

    
    int lockBuffer = 0;
    int firstElem = myOutputOffset / 32;
    int lastElem = (myOutputOffset + myOutputLength - 1) / 32;
    for(int i = 0; i < batchSize; i++) {
        int thisElemFirst = myOutputOffset / 32;
        int thisElemLast = (myOutputOffset + codeTable[fin[batchOffset + i]].codeLen - 1) / 32;
        if(thisElemFirst == firstElem || thisElemLast == lastElem)
            lockBuffer = 1;
        else
            lockBuffer = 0;
        writeCodeToBuffer(fout, &myOutputOffset, codeTable[fin[batchOffset + i]], lockBuffer);
    }

    pthread_exit(0);
}

// --------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // parse command line args
    if(argc != 4 || (argc > 1 && strcmp(argv[1], "--help") == 0)) {
        fprintf(stderr, "Usage: %s <input text file> <output file name> <number of threads>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char* filename = argv[1];
    char* outfilename = argv[2];
    numThreads = atoi(argv[3]);

    double start, end;
    struct timeval timecheck;

    gettimeofday(&timecheck, NULL);
    start = timecheck.tv_sec + (double)timecheck.tv_usec / 1000000;

    // initialize thread variables
    pthread_t threads[numThreads];
    pthread_attr_t threadAttr;
    pthread_attr_init(&threadAttr);
    threadIDList = malloc(sizeof(int) * numThreads);
    for(int i = 0; i < numThreads; i++)
        threadIDList[i] = i;

    // map input file
    int in_fd = open(filename, O_RDONLY);
    inputFileSize = lseek(in_fd, 0, SEEK_END);
    lseek(in_fd, 0, SEEK_SET);
    fin = (uint8_t*) mmap(NULL, inputFileSize, PROT_READ, MAP_PRIVATE, in_fd, 0);

    // measure char frequency for ASCII set
    pthread_mutex_init(&charFreqMutex, NULL);
    charFreqThreads = malloc(sizeof(int*) * numThreads);
    for(int i = 0; i < numThreads; i++) {
        charFreqThreads[i] = malloc(sizeof(int) * 256);
        memset(charFreqThreads[i], 0, sizeof(int) * 256);
        pthread_create(&threads[i], &threadAttr, getCharFreq, &threadIDList[i]);
    }
    for(int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    // construct huffman tree
    HuffmanNode* root = huffman(charFreq);

    // get char encoding scheme by traversing huffman tree
    for(int i = 0; i < 256; i++) {
        codeTable[i].codeLen = -1;
    }
    populateCodeTable(codeTable, root);

    // map output file
    int outputFileSize = 72 + 32 * numThreads;
    for(int i = 0; i < 256; i++) {
        if(codeTable[i].codeLen > 0) {
            outputFileSize += 16 + codeTable[i].codeLen;
        }
        outputFileSize += charFreq[i] * codeTable[i].codeLen;
    }
    outputFileSize = ceil((double) outputFileSize / 32) * sizeof(uint32_t);
    int out_perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    int out_fd = open(outfilename, O_CREAT | O_RDWR, out_perm);
    ftruncate(out_fd, outputFileSize);
    fout = (uint32_t*) mmap(NULL, outputFileSize, PROT_READ | PROT_WRITE, MAP_SHARED, out_fd, 0);
    memset(fout, 0, outputFileSize);

    // write encoding scheme as header to output file
    int bitOffset = 0;
    encodeHeader(fout, &bitOffset, inputFileSize, codeTable);

    // encode file contents
    outputOffset = (int*) malloc(sizeof(int) * numThreads);
    outputLength = (int*) malloc(sizeof(int) * numThreads);
    outputCond = (pthread_cond_t*) malloc(sizeof(pthread_cond_t) * numThreads);
    outputMutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t) * numThreads);
    for(int i = 0; i < numThreads; i++) {
        outputLength[i] = outputOffset[i] = -1;
        pthread_mutex_init(&outputMutex[i], NULL);
        pthread_cond_init(&outputCond[i], NULL);
    }
    pthread_mutex_init(&foutMutex, NULL);
    outputOffset[0] = bitOffset;

    for(int i = 0; i < numThreads; i++) {
        pthread_create(&threads[i], &threadAttr, encodeBatch, &threadIDList[i]);
    }
    for(int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    // add thread offset values to header
    for(int i = 0; i < numThreads; i++) {
        fout[2 + i] = (uint32_t) outputOffset[i];
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