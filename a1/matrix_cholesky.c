#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

double** allocateMatrix(int n, int m);
double** transpose(double** A, int n);
void forwardSubstitution(double** U, double* x, double* y, int n);
void backSubstitution(double** U, double* x, double* y, int n);

double** allocateMatrix(int n, int m) {
    double** A = malloc(n * sizeof(double*));
    for(int i = 0; i < n; i++)
        A[i] = malloc(m * sizeof(double));
    return A;
}

double** transpose(double** A, int n) {
    double** At = allocateMatrix(n, n);
    for(int i = 0; i < n; i++)
        for(int j = 0; j < n; j++)
            At[j][i] = A[i][j];
    return At;
}

void forwardSubstitution(double** U, double* x, double* y, int n) {
    for(int k = 0; k < n; k++) {
        x[k] = y[k] / U[k][k];
        for(int i = k + 1; i < n; i++) {
            y[i] -= U[i][k] * x[k];
        }
    }
}

void backSubstitution(double** U, double* x, double* y, int n) {
    for(int k = n - 1; k >= 0; k--) {
        x[k] = y[k] / U[k][k];
        for(int i = k - 1; i >= 0; i--) {
            y[i] -= U[i][k] * x[k];
        }
    }
}

int main(int argc, char* argv[]) {
    // initialize MPI
    int rank;
    int comm_sz;
    double my_time, max_time;
    MPI_Init(&argc, &argv);
    my_time = MPI_Wtime();
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);

    // read input and distribute rows among processes, using cyclic assignment
    // each row i gets mapped to process i % comm_sz
    int n;
    if(rank == 0) {
        printf("> n (size of system of equations): ");
        fflush(stdout);
        scanf("%d", &n);
        printf("Solving system Ax=B for x...\n");
    } 
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int my_n = n / comm_sz + ((rank < n % comm_sz) ? 1 : 0);
    double** my_A = allocateMatrix(my_n, n);

    if(rank == 0) {
        printf("> A (n x n matrix): \n");
        double** A = allocateMatrix(n, n);
        for(int i = 0; i < n; i++) {
            for(int j = 0; j < n; j++) {
                scanf("%lf", &A[i][j]);
            }
            if(i % comm_sz != 0)
                MPI_Send(A[i], n, MPI_DOUBLE, i % comm_sz, 0, MPI_COMM_WORLD);
            else {
                for(int j = 0; j < n; j++) {
                    my_A[i / comm_sz][j] = A[i][j];
                }
            }
        }
    } else {
        for(int j = 0; j < my_n; j++) 
            MPI_Recv(my_A[j], n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    // CHOLESKY FACTORIZTION
    double reduce_row[n];
    for(int k = 0; k < n; k++) {
        // Computation on row k
        if(rank == k % comm_sz) {
            int my_i = k / comm_sz;
            my_A[my_i][k] = sqrt(my_A[my_i][k]);
            for(int j = k + 1; j < n; j++) {
                my_A[my_i][j] /= my_A[my_i][k];
            }
            for(int j = k; j < n; j++) {
                reduce_row[j] = my_A[my_i][j];
            }
        }
        // Broadcast row k
        MPI_Bcast(reduce_row + k + 1, n - k - 1, MPI_DOUBLE, k % comm_sz, MPI_COMM_WORLD);
        // Reduction in rows k+1, ..., n using row k
        for(int i = k + 1; i < n; i++) {
            if(rank == i % comm_sz) {
                int my_i = i / comm_sz;
                for(int j = i; j < n; j++) {
                    my_A[my_i][j] -= reduce_row[i] * reduce_row[j];
                }
            }
        }
    }

    // Collect result from all processes
    if(rank != 0) {
        for(int j = 0; j < my_n; j++) {
            MPI_Send(my_A[j], n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
        }
    } else {
        double** A = allocateMatrix(n, n);
        for(int i = 0; i < n; i++) {
            if(i % comm_sz == 0) {
                for(int j = 0; j < n; j++)
                    A[i][j] = my_A[i / comm_sz][j];
            } else {
                MPI_Recv(A[i], n, MPI_DOUBLE, i % comm_sz, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            for(int j = 0; j < i; j++) {
                A[i][j] = 0;
            }
        }
        // solve system using forward and back substitution
        double** Uupper = A;
        double** Ulower = transpose(Uupper, n);
        double* B = malloc(n * sizeof(double));
        printf("> B (vector of length n): \n");
        for(int i = 0; i < n; i++) {
            scanf("%lf", &B[i]);
        }
        double* y = malloc(n * sizeof(double));
        forwardSubstitution(Ulower, y, B, n);
        double* x = malloc(n * sizeof(double));
        backSubstitution(Uupper, x, y, n);
        // print solution
        printf("x (solution):\n");
        for(int i = 0; i < n; i++) {
            printf("x%d = %lf\n", i+1, x[i]);
        }
    }

    my_time = MPI_Wtime() - my_time;
    MPI_Reduce(&my_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if(rank == 0) {
        printf("Time: %lf sec\n", max_time);
    }

    MPI_Finalize();
}