#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

typedef struct edge 
{
  int src;
  int dest;
  int weight;
}Edge;

int *init_Components(int *components, int size) {
  if (!components) 
	  components = (int*)malloc(size * sizeof(int));
  
  for (int i = 0; i < size; i++)
    components[i] = -1;
  return components;
}

void rename_structs(int *sets,int src, int dest, int size) 
{
  for (int i = 0; i < size; i++)
    if (sets[i] == src) sets[i] = dest;
}

void components_merge(int *List1, int *List2, int size, Edge *edges) 	//A normal uninon operation on two sets/lists
{

  for (int i = 0; i < size; i++)
  {
	if(List1[i] < 0)
		List1[i] = List2[i];
	else if (List2[i] < 0 || edges[List1[i]].weight < edges[List2[i]].weight)
		continue;	
	else
	List1[i] = List2[i];
  }
}

void components_find(int *components, int *vertices, Edge *edges, int begin, int end) // A generic find_set operation on an element and set along with finding a minimum edges
{
	for (int i = begin; i < end; i++) 
	{
		Edge cur_edge = edges[i];
		int set_source = vertices[cur_edge.src];
		int set_target = vertices[cur_edge.dest];
		if (set_source == set_target) 
			continue;
		
		int *target = &components[set_target];
		int *source = &components[set_source];
	
		if (*target < 0 || cur_edge.weight < edges[*target].weight) 
			*target = i;
		
		if (*source < 0 || cur_edge.weight < edges[*source].weight) 
			*source = i;
	}
}


int components_update(int set_count,int *components, int components_count, int *vertices, Edge *edges) 
{
  int i, combined = 0;
  for (i = 0; i < components_count; i++) 
  {
    int component = components[i];
    if (component < 0) continue;
    Edge edge = edges[component];
    int source = vertices[edge.src];
    int target = vertices[edge.dest];
    if (source == target) continue;
    
	int src, dest;
	if(source > target)
	{
		src = source;
		dest = target;
	}
	else
	{
		src = target;
		dest = source;
	}
	rename_structs(vertices, src,dest, set_count);
    combined++;
  }
  return combined;
}
int sets_update(int *sets, int set_count,int *components, int components_count, int *vertices, Edge *edges) 
{
  int i, combined = 0;
  for (i = 0; i < components_count; i++) 
  {
    int component = components[i];
    if (component < 0) continue;
    Edge edge = edges[component];
    int source = vertices[edge.src];
    int destination = vertices[edge.dest];
    if (source == destination) continue;
    
	int src, dest;
	if(source > destination)
	{
		src = source;
		dest = destination;
	}
	else
	{
		src = destination;
		dest = source;
	}
	rename_structs(vertices, src,dest, set_count);
    rename_structs(sets, src,dest,set_count);
    sets[component] = dest;
    combined++;
  }
  return combined;
}
int* boruvka_mst(int vertex_count, Edge *edges, int edge_count, int rank , int size) 
{
  int *vertices = (int*)malloc(vertex_count * sizeof(int)) ;
  int *sets = (int*)malloc(edge_count * sizeof(int));
  int *components_recv = (int*)malloc(vertex_count* sizeof(int));
  
  for (int i = 0; i < vertex_count; i++)
  {
	  vertices[i] = i;	  
  }
  for (int i = 0; i < edge_count; i++)
  {
	  sets[i] = -1;
  }
  
  int* edge_starts = malloc( (rank+2) * sizeof(int));
  edge_starts[0] = 0;
  for(int i =1;i <= rank+1; i++)
  {
	edge_starts[i] = edge_starts[i-1] + edge_count/size + ( edge_count%size > i-1);    
  }
	  
  int edge_begin = edge_starts[rank];
  int edge_end = edge_starts[rank + 1];
  free(edge_starts);
  //printf("My worldrank is %d and my edge_begin is %d and edge_end is %d\n", rank, edge_begin, edge_end);
  
  int *components = NULL, i; 
  int component_count = vertex_count, combined = 0;
  do 
  {
    components = init_Components(components, component_count);
    components_find(components, vertices, edges, edge_begin, edge_end);
    if (rank == 0)
		{
			for (int i=1; i<size; i++) 
			{
				MPI_Recv(components_recv, component_count, MPI_INT, MPI_ANY_SOURCE,0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				components_merge(components, components_recv, component_count, edges);				
			}
			MPI_Bcast(components, component_count, MPI_INT, 0, MPI_COMM_WORLD);
			combined = sets_update(sets, edge_count, components, component_count, vertices, edges);
		}		
	else 
	{	
		MPI_Send(components, component_count, MPI_INT, 0, 0, MPI_COMM_WORLD);
		MPI_Bcast(components, component_count, MPI_INT, 0, MPI_COMM_WORLD);
		combined = components_update(edge_count, components, component_count, vertices, edges);
    }
	component_count -= combined;
  }while (combined > 0);
  
  
  free(components_recv);
  free(components);
  free(vertices);
  return sets;
}



int main(int argc, char *argv[]) 
{
  MPI_Init(NULL, NULL);
  
  int rank, size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  int vertex_count, edge_count, i;
  
  if (rank == 0)
  { printf("Please input the number of vertices and egdges in the input Graph seprated by a single space\n")	;  
	scanf("%d %d", &vertex_count, &edge_count);
  }
  MPI_Bcast(&vertex_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&edge_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
  
  Edge *edges = (Edge*)malloc(edge_count * sizeof(Edge));
  
  if (rank == 0)
  {
	  printf("Please input the edges in the format of source destination weight each seperated by a single space\n");
	  for (i = 0; i < edge_count; i++) 
	{
		scanf("%d %d %d", &edges[i].src, &edges[i].dest, &edges[i].	weight);
		edges[i].src--; 			// accounting for different indexing
		edges[i].dest--;			// same as above
	}
  }
  MPI_Bcast((int*)edges, edge_count*3, MPI_INT, 0, MPI_COMM_WORLD);
  
  double start = MPI_Wtime();
  int *sets = boruvka_mst(vertex_count, edges, edge_count,rank,size);
  
  int MST_edge_count = 0, weight = 0;
  for (i = 0; i < edge_count; i++) 
  {
    if (sets[i] < 0) continue;
    weight += edges[i].weight;
    MST_edge_count++;
  }
  if (rank == 0) {
	if(MST_edge_count != vertex_count - 1)
	{
		printf("The MST does not contain all the Vertices, This maybe because of an input of disconnected graph!\n");
		MPI_Finalize();
		return 0;
	}
    printf("The MST is given below where each line depicts\nsource\tdest\tweight\n");
	for (i = 0; i < edge_count; i++) 
	{
      if (sets[i] < 0) 

		  continue;
      Edge edge = edges[i];
      printf("%d\t%d\t%d\n", edge.src + 1, edge.dest + 1, edge.weight);
    }
  }
  double elapsed = MPI_Wtime() - start;
  double max;
  MPI_Reduce (&elapsed, &max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
	if (rank == 0)
    printf("The Weight of MST is %d\nTime taken is %lf sec with %d processes\n", weight, max, size);
  
  MPI_Finalize();
  return 0;
}