#include <ctype.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD_SEP " .,!;-\"\':"
#define MAX_QUERY_LEN 2000
#define MAX_TERM_LENGTH 100

int tokenizeData(int max_size, char* buf, char*** dataTokenized) {
    char* token;
    int wcount = 0;
    token = strtok(buf, WORD_SEP);
    *dataTokenized = (char**)malloc(sizeof(char*) * max_size / 2);
    while (token != NULL) {
        (*dataTokenized)[wcount] = (char*)malloc(sizeof(char) * (strlen(token) + 1));
        strcpy((*dataTokenized)[wcount], token);
        token = strtok(NULL, WORD_SEP);
        wcount++;
    }
    return wcount;
}

int strcmpi(char* s1, char* s2) {
    int i;

    if (strlen(s1) != strlen(s2))
        return -1;

    for (i = 0; i < strlen(s1); i++) {
        if (toupper(s1[i]) != toupper(s2[i]))
            return s1[i] - s2[i];
    }
    return 0;
}

int matches(char* s1, char* s2, int* lineOffset) {
    // printf("Matching %s and %s\n", s1, s2);
    while (s1[0] == '\n') {
        ++(*lineOffset);
        strcpy(&s1[0], &s1[1]);
    }
    return strcmpi(s1, s2);
}

int main(int argc, char* argv[]) {
    char* filename = "./hello.txt";

    int numtasks, rank;
    MPI_Init(&argc, &argv);
    double start = MPI_Wtime();
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_File fptr;
    MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &fptr);
    int chunksize;
    MPI_Offset filesize;
    if (rank == 0) {
        MPI_File_get_size(fptr, &filesize);
        printf("Filesize: %d bytes\n", filesize);
        chunksize = filesize / numtasks;
    }
    MPI_Bcast(&filesize, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&chunksize, 1, MPI_INT, 0, MPI_COMM_WORLD);
    // MPI_Barrier(MPI_COMM_WORLD);

    int idx = (rank + 1) * chunksize;
    if (idx < filesize && rank != numtasks - 1) {
        MPI_File_seek(fptr, idx, MPI_SEEK_SET);
        char* buf = (char*)malloc(sizeof(char) * 2);
        MPI_File_read(fptr, buf, 1, MPI_CHAR, NULL);
        buf[1] = '\0';
        while (buf[0] != ' ') {
            MPI_File_read(fptr, buf, 1, MPI_CHAR, NULL);
            buf[1] = '\0';
        }
    }
    // MPI_Barrier(MPI_COMM_WORLD);
    MPI_Offset final_mark;
    MPI_Offset initial_mark = 0;
    if (rank != numtasks - 1) {
        MPI_File_get_position(fptr, &final_mark);
    } else {
        MPI_File_seek(fptr, 0, MPI_SEEK_END);
        MPI_File_get_position(fptr, &final_mark);
    }
    if (rank != numtasks - 1) {
        MPI_Send(&final_mark, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
    }
    if (rank != 0) {
        MPI_Recv(&initial_mark, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, NULL);
        // initial_mark += 1;
    }
    // printf("I'm proc no. %d and my byte to read is %d\n", rank, initial_mark);
    int bytesToRead = final_mark - initial_mark + 1;
    char* fileData = (char*)malloc((bytesToRead + 1) * sizeof(char));
    MPI_File_seek(fptr, initial_mark, MPI_SEEK_SET);
    MPI_File_read(fptr, fileData, bytesToRead, MPI_CHAR, NULL);
    fileData[bytesToRead - 1] = '\0';
    // printf("I'm proc no. %d and my byte to read is %d data read is %s\n", rank, bytesToRead, fileData);

    char** dataTokenized;
    int wordCount = tokenizeData(bytesToRead, fileData, &dataTokenized);
    // for (int i = 0; i < wordCount; ++i) {
    //     printf("%s1 ", dataTokenized[i]);
    // }

    int andor;
    char* qwords[MAX_QUERY_LEN / MAX_TERM_LENGTH];
    int qwordcount = 0;
    int andorcount = 0;
    if (rank == 0) {
        char* buf;
        buf = malloc(sizeof(char) * MAX_QUERY_LEN);
        printf("Enter query set of words: \n");
        fgets(buf, MAX_QUERY_LEN, stdin);
        char* token;
        buf[strlen(buf) - 1] = '\0';
        token = strtok(buf, WORD_SEP);
        while (token != NULL) {
            if (strcmpi("AND", token) == 0) {
                andor = 1;
            } else if (strcmpi("OR", token) == 0) {
                andor = 0;
            } else {
                qwords[qwordcount] = malloc(sizeof(char) * (strlen(token) + 1));
                strcpy(qwords[qwordcount], token);
                if (qwords[qwordcount][strlen(token)] != '\0') {
                    qwords[qwordcount][strlen(token)] = '\0';
                }
                // printf("For %s Size is %d\n", qwords[qwordcount], strlen(qwords[qwordcount]));
                ++qwordcount;
            }
            token = strtok(NULL, WORD_SEP);
        }
        printf("Initiating search...\n");
    }
    MPI_Bcast(&qwordcount, 1, MPI_INT, 0, MPI_COMM_WORLD);
    for (int i = 0; i < qwordcount; ++i) {
        if (rank != 0) {
            qwords[i] = malloc(sizeof(char) * (MAX_TERM_LENGTH));
        }
    }
    // MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < qwordcount; ++i) {
        // printf("For %s Size is %d\n", qwords[i], strlen(qwords[i]));
        MPI_Bcast(qwords[i], MAX_TERM_LENGTH, MPI_CHAR, 0, MPI_COMM_WORLD);
        // printf("I'm proc %d and my word is %s\n", rank, qwords[i]);
    }
    // MPI_Barrier(MPI_COMM_WORLD);

    int numLinesOffset = 0;
    for (int i = 0; i < wordCount; ++i) {
        for (int j = 0; dataTokenized[i][j] == '\n'; ++j) {
            ++numLinesOffset;
            // printf("%s", dataTokenized[i]);
            // strcpy(&dataTokenized[i][0], &dataTokenized[i][1]);
        }
    }
    if (rank == numtasks - 1) {
        numLinesOffset++;
    } else {
        MPI_Send(&numLinesOffset, 1, MPI_INT, rank + 1, 1, MPI_COMM_WORLD);
    }
    if (rank != 0) {
        MPI_Recv(&numLinesOffset, 1, MPI_INT, rank - 1, 1, MPI_COMM_WORLD, NULL);
    } else {
        numLinesOffset = 0;
    }
    MPI_Scan(&numLinesOffset, &numLinesOffset, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    // MPI_Barrier(MPI_COMM_WORLD);
    // printf("For proc %d, numLinesOffset = %d\n", rank, numLinesOffset);
    int wordFind[qwordcount];
    int lineLoc[qwordcount];
    int wordLoc[qwordcount];
    for (int i = 0; i < qwordcount; ++i) {
        wordFind[i] = 0;
        lineLoc[i] = 0;
        wordLoc[i] = 0;
        int wordReset = 1;
        int lineOffset = 1 + numLinesOffset;
        for (int j = 0; j < wordCount; ++j) {
            int oldOffset = lineOffset;
            if (matches(dataTokenized[j], qwords[i], &lineOffset) == 0) {
                // printf("For proc %d, word of %s wordLoc is %d and lineLoc is %d. OldOffset is %d and lineOffset is %d.\n", rank, qwords[i], wordLoc[i], lineLoc[i], oldOffset, lineOffset);
                wordLoc[i] = wordReset;
                wordFind[i] = 1;
                lineLoc[i] = lineOffset;
                break;
            }
            if (oldOffset != lineOffset) {
                wordReset = 1;
            }
            wordReset++;
        }
        // printf("For proc %d, word of %s wordLoc is %d and lineLoc is %d\n", rank, qwords[i], wordLoc[i], lineLoc[i]);
    }
    // MPI_Barrier(MPI_COMM_WORLD);
    MPI_Send(wordLoc, qwordcount, MPI_INT, 0, 2, MPI_COMM_WORLD);
    MPI_Send(lineLoc, qwordcount, MPI_INT, 0, 3, MPI_COMM_WORLD);
    int lineLocAgg[numtasks][qwordcount];
    int wordLocAgg[numtasks][qwordcount];
    if (rank == 0) {
        for (int i = 0; i < numtasks; i++) {
            MPI_Recv(wordLocAgg[i], qwordcount, MPI_INT, i, 2, MPI_COMM_WORLD, NULL);
            MPI_Recv(lineLocAgg[i], qwordcount, MPI_INT, i, 3, MPI_COMM_WORLD, NULL);
        }
    }
    if (rank == 0) {
        MPI_Reduce(MPI_IN_PLACE, wordFind, qwordcount, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    } else {
        MPI_Reduce(wordFind, wordFind, qwordcount, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    // MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        printf("\n");
        long long prod = andor;
        for (int i = 0; i < qwordcount; ++i) {
            // printf("WordFind at %d is %d wordLoc is %d and lineLoc is %d.\n", i, wordFind[i], wordLoc[i], lineLoc[i]);
            if (andor == 1) {
                prod *= wordFind[i];
            } else {
                prod += wordFind[i];
            }
        }
        if (prod == 0) {
            printf("Search query not found.\n");
        } else {
            printf("Search query was found.\n");
            for (int i = 0; i < qwordcount; ++i) {
                for (int j = 0; j < numtasks; ++j) {
                    if (wordLocAgg[j][i] != 0 && lineLocAgg[j][i] != 0 && wordFind[i] != 0) {
                        printf("Word %s was found at Line number %d and is the %dth word in that line.\n", qwords[i], lineLocAgg[j][i], wordLocAgg[j][i]);
                        break;
                    }
                }
            }
        }
    }
    double end = MPI_Wtime();
    MPI_Barrier(MPI_COMM_WORLD);
    double total_time;
    double total = end - start;
    MPI_Reduce(&total, &total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        printf("Time elapsed = %lfs\n", total_time);
    }

    MPI_Finalize();
    return 0;
}