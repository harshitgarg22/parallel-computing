Usage of the executable is 
mpirun -np 4 ./mst_sollin  < {input file location}
or 
mpirun -np 4 ./mst sollin

The input must be in the following format
First line must contain the Vertex_count and edge_count seperated by a space
Next edge_count line contain the edges in the following format 
source_vertex dest_vertex weight 
each seperated by a space

Certain input files have already been provided.