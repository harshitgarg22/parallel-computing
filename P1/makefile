all:
	mpicc search_parallel_new.c -o search_parallel_new.out

run:
	mpiexec -np 4 --oversubscribe ./search_parallel_new.out 

rerun:
	mpicc search_parallel_new.c -o search_parallel_new.out
	mpiexec -np 4 --oversubscribe ./search_parallel_new.out 

debug:
	mpicc search_parallel_new.c -o search_parallel_new.out
	mpirun -np 2 --oversubscribe --mca orte_base_help_aggregate 0 ./search_parallel_new.out 
