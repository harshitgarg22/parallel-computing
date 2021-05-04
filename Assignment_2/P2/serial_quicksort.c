#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
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

void generateArray(int arr[], int n) {
    // randomly fills up an array
    // printf("Original array: ");
    for (int i = 0; i < n; i++) {
        arr[i] = (rand() % (n * 2));
        // printf("%d ", arr[i]);
    }
    // printf("\n");
}

int main() {
    int n = 1024;
    int* arr;
    arr = (int*)malloc(sizeof(int) * n);
    printf("Num of elements is %d\n", n);
    generateArray(arr, n);

    struct timeval tv_initial;
    gettimeofday(&tv_initial, NULL);
    quicksort(arr, 0, n - 1);
    struct timeval tv_final;
    gettimeofday(&tv_final, NULL);

    double time_taken = tv_final.tv_sec * 1000 - tv_initial.tv_sec * 1000 + tv_final.tv_usec / 1000.0 - tv_initial.tv_usec / 1000.0;
    printf("Time taken for sequential = %lfms\n", time_taken);

    // for (int i = 0; i < n; ++i) {
    //     printf("%d ", arr[i]);
    // }
    // printf("\n");
}