#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define PIVOT_PROC 0
#define THREAD_COUNT 4
#define MAX_N 1024

void die(char* s) {
    perror(s);
    exit(-1);
}

void generateArray(int arr[], int n) {
    // randomly fills up an array
    // printf("Original array: \n");
    for (int i = 0; i < n; i++) {
        arr[i] = (rand() % (n * 2) + 1);
        // printf("%d ", arr[i]);
    }
    // printf("\n\n\n");
}

void quicksort(int arr[], int l, int r) {
    int i, j, pivot, temp;

    if (l < r) {
        pivot = l;
        i = l;
        j = r;

        while (i < j) {
            while (arr[i] <= arr[pivot] && i < r)
                i++;
            while (arr[j] > arr[pivot])
                j--;
            if (i < j) {
                temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }

        temp = arr[pivot];
        arr[pivot] = arr[j];
        arr[j] = temp;
        quicksort(arr, l, j - 1);
        quicksort(arr, j + 1, r);
    }
}

int pivot[THREAD_COUNT];
int pivotidx[THREAD_COUNT];

int main(int argc, char* argv[]) {
    if (argc != 2) {
        die("Incorrect arguments");
    }
    int n = atoi(argv[1]);  // number of elements in array
    int* arr;
    arr = (int*)malloc(sizeof(int) * n);
    printf("Num of elements is %d\n", n);
    generateArray(arr, n);
    int threadCount = THREAD_COUNT;

    int** thread_arr = malloc(sizeof(int) * THREAD_COUNT * MAX_N);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        thread_arr[i] = malloc(sizeof(int) * MAX_N);
    }
    double time_taken;
    struct timeval tv_initial;
    gettimeofday(&tv_initial, NULL);

#pragma omp parallel num_threads(THREAD_COUNT)
    {
        // omp_set_num_threads(THREAD_COUNT);
        int threadNum = omp_get_thread_num();
        // printf("%d\n", omp_get_num_threads());
        int sizeShare = n / threadCount;
        int size = n / threadCount;
        memset(thread_arr[threadNum], -1, sizeof(int) * MAX_N);
        int l = threadNum * (size);
        // #pragma omp barrier
        // #pragma omp for
        for (int i = 0; i < size; ++i) {
            thread_arr[threadNum][i] = arr[l + i];
            // printf("%d ", thread_arr[threadNum][i]);
        }

        // printf("Hello I'm %d sorting at %d to %d\n", omp_get_thread_num(), threadNum * (n / threadCount), (threadNum + 1) * (n / threadCount));
        quicksort(thread_arr[threadNum], 0, size - 1);
        // pivot[threadNum] = thread_arr[threadNum][(size + 1) / 2 - 1];
        int count = log(threadCount) / log(2);
        int mask = 1 << (count - 1);
        int isLower = 0;
        for (int i = count - 1; i >= 0; --i, mask = 1 << i) {
            // #pragma omp single
            //             printf("\nFOR i=%d\n\n", i);
            // #pragma omp single
            //             for (int i = 0; i < THREAD_COUNT; ++i) {
            //                 printf("%d: ", i);
            //                 for (int j = 0; j < MAX_N; ++j) {
            //                     if (thread_arr[i][j] != -1) {
            //                         printf("%d ", thread_arr[i][j]);
            //                     }
            //                 }
            //                 printf("\n");
            //             }
            int shareWith = mask ^ threadNum;  // Thread number to share with
            isLower = ((threadNum & mask) == 0) ? 1 : 0;
            // size = 0;
            // for (int j = 0; thread_arr[threadNum][j] != -1; ++j) {
            //     ++size;
            // }
            sizeShare = 0;
            for (int j = 0; thread_arr[shareWith][j] != -1; ++j) {
                ++sizeShare;
            }
            // printf("SIZESHARE FOR THREAD %d is %d where share thread %d\n", threadNum, sizeShare, shareWith);
            // printf("For proc %d, isLower: %d\n", threadNum, isLower);
            // printf("I'm %d sharing with %d\n", threadNum, shareWith);
            // printf("size=%d\n", size);
            // printf("sizeShare=%d\n", sizeShare);
            // #pragma omp barrier
            if (isLower == 1) {
                pivot[threadNum] = ((sizeShare % 2) && (i % 2 == 1) != 0) ? thread_arr[shareWith][(sizeShare) / 2] : (thread_arr[shareWith][(sizeShare - 1) / 2] + thread_arr[shareWith][(sizeShare - 1) / 2]) / 2;
            } else {
                pivot[threadNum] = ((size % 2) && (i % 2 == 1) != 0) ? thread_arr[threadNum][(size) / 2] : (thread_arr[threadNum][(size - 1) / 2 - 1] + thread_arr[threadNum][(size) / 2]) / 2;
            }
            int my_pivotidx;
            // if (i == count - 1) {
            //     pivot[threadNum] = ((size % 2) != 0) ? thread_arr[PIVOT_PROC][(size) / 2 - 1] : (thread_arr[PIVOT_PROC][(size - 1) / 2 - 1] + thread_arr[PIVOT_PROC][(10) / 2 - 1]) / 2;
            // }
            for (int j = 0; j < size; ++j) {
                if (pivot[threadNum] <= thread_arr[threadNum][j]) {
                    pivotidx[threadNum] = j;
                    my_pivotidx = j;
                    break;
                }
            }
            // printf("%d Mypivot for %d is %d\n", pivot[threadNum], threadNum, my_pivotidx);
            int save_send[my_pivotidx];
            int save_mine[size - my_pivotidx];

#pragma omp barrier
            // Upper half of proc saving the lower list to be sent later
            if (isLower == 0) {
                // printf("For %d Savesend: \n", threadNum);
                for (int j = 0; j < my_pivotidx; ++j) {
                    save_send[j] = thread_arr[threadNum][j];
                    // printf("ss-%d-%d ", threadNum, save_send[j]);
                }
                // printf("\n");
                // printf("\nFor %d Savemine: \n", threadNum);
                for (int j = my_pivotidx; j < size; ++j) {
                    save_mine[j - my_pivotidx] = thread_arr[threadNum][j];
                    // printf("sm-%d-%d ", threadNum, save_mine[j - my_pivotidx]);
                }
                // printf("\n");
                memset(thread_arr[threadNum], -1, sizeof(int) * MAX_N);
            }

#pragma omp barrier

            // Lower half of proc giving away higher list
            if (isLower == 1) {
                for (int j = 0; j < my_pivotidx; ++j) {
                    save_mine[j] = thread_arr[threadNum][j];
                    // printf("sm-%d-%d ", threadNum, save_mine[j]);
                }
                // printf("\n");
                for (int j = my_pivotidx; j < size; ++j) {
                    save_send[j - my_pivotidx] = thread_arr[threadNum][j];
                    // printf("ss-%d-%d ", threadNum, save_send[j - my_pivotidx]);
                }
                // printf("\n");
                memset(thread_arr[threadNum], -1, sizeof(int) * MAX_N);
                for (int j = 0; j < size - my_pivotidx; ++j) {
                    thread_arr[shareWith][j] = save_send[j];
                }
            }
#pragma omp barrier

            // Upper half will now share its lower list to lower half proc
            if (isLower == 0) {
                for (int j = 0; j < size - my_pivotidx; ++j) {
                    thread_arr[threadNum][j + sizeShare - pivotidx[shareWith]] = save_mine[j];
                    // thread_arr[shareWith][j] = save[j];
                }
                for (int j = 0; j < my_pivotidx; ++j) {
                    thread_arr[shareWith][j] = save_send[j];
                }
                size = size - my_pivotidx + sizeShare - pivotidx[shareWith];
                // printf("For thread %d, size is %d\n", threadNum, size);
                quicksort(thread_arr[threadNum], 0, size - 1);
            }
#pragma omp barrier

            if (isLower == 1) {
                for (int j = 0; j < my_pivotidx; ++j) {
                    thread_arr[threadNum][j + pivotidx[shareWith]] = save_mine[j];
                    // thread_arr[shareWith][j] = save[j];
                }
                size = my_pivotidx + pivotidx[shareWith];
                // printf("For thread %d, size is %d\n", threadNum, size);
                quicksort(thread_arr[threadNum], 0, size - 1);
            }
        }
    }

    struct timeval tv_final;
    gettimeofday(&tv_final, NULL);
    printf("Sorted array is: \n");
    for (int i = 0; i < THREAD_COUNT; ++i) {
        for (int j = 0; j < MAX_N; ++j) {
            if (thread_arr[i][j] != -1) {
                printf("%d ", thread_arr[i][j]);
            }
        }
        // printf("\n");
    }
    printf("\n");
    time_taken = tv_final.tv_sec * 1000 - tv_initial.tv_sec * 1000 + tv_final.tv_usec / 1000.0 - tv_initial.tv_usec / 1000.0;
    printf("Time taken for %d threads = %lfms\n", threadCount, time_taken);
}