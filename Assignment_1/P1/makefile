all:
	mpicc search_parallel.c -o search_parallel.out

run:
	mpiexec -np 4 --oversubscribe ./search_parallel.out 

rerun:
	mpicc search_parallel.c -o search_parallel.out
	mpiexec -np 4 --oversubscribe ./search_parallel.out --mca opal_warn_on_missing_libcuda 0

debug:
	mpicc search_parallel.c -o search_parallel.out
	mpirun -np 2 --oversubscribe --mca orte_base_help_aggregate 0 ./search_parallel.out 

iterate:
	mpicc -g search_parallel.c -o search_parallel.out 
	for i in {1..10}; do	\
		mpiexec --oversubscribe -n $$i ./search_parallel.out;	\
	done