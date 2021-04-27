compfile = comp.txt
outfile = output.txt
file = input10.txt 
threads = 1

all: encode_parallel decode_parallel

encode_parallel: encode_parallel.c
	gcc -o encode_parallel encode_parallel.c -lm -lpthread

decode_parallel: decode_parallel.c
	gcc -o decode_parallel decode_parallel.c -lm -lpthread

clean:
	rm encode_parallel decode_parallel

test: encode_parallel decode_parallel
	./encode_parallel $(file) $(compfile) $(threads)
	./decode_parallel $(compfile) $(outfile)
	diff $(file) $(outfile)
	rm $(compfile) $(outfile)
