Compile using

make 
OR
mpicc -o matrix_cholesky matrix_cholesky.c -lm

Execute using

mpirun -np 4 ./matrix_cholesky < input100.txt

Files with input of various sizes are also provided.