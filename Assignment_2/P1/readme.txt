Compile using 
    make

Run using
    ./encode_parallel <input ASCII file> <output encoded file> <number of threads>
    ./decode_parallel <input encoded file> <output ASCII file> 

Test using 
    make test file=<input file name> threads=<number of threads>

    Encodes given file using given number of threads, decodes the resulting file.
    Compares output file with the original one (using diff) and removes encoded and decoded files.
