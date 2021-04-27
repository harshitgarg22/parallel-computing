#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#define checkCudaError(o, l) _checkCudaError(o, l, __func__)
#define SHARED_MEMORY_BANKS 32
#define LOG_MEM_BANKS 5
#define CONFLICT_FREE_OFFSET(n) ((n) >> LOG_MEM_BANKS)
#include<limits.h>

long long int THREADS_PER_BLOCK = 512;		// This is the numver of threads per block used, and 512 gave the best results
long long int ELEMENTS_PER_BLOCK = THREADS_PER_BLOCK * 2;	// As each threads takes care of two elements so number of elements is twice of threads

float sequential_scan(long long int* output, long long int* input, long long int length,long long int operation);
float scan(long long int *output, long long int *input, long long int length,long long int operation);
void scanMultiBlock(long long int *output, long long int *input, long long int length,long long int operation,long long int identity);
void scanSingleBlock(long long int *device_output, long long int *device_input, long long int length, long long int operation,long long int identity);
void scanBlockSizedArray(long long int *output, long long int *input, long long int length, long long int operation,long long int identity);
void check(long long int* CPU_Vector,long long int* GPU_Vector, long long int start, long long int end);

__global__ void prescan_SingleBlock(long long int *output, long long int *input, long long int n, long long int nextPowerOfTwo, long long int operation, long long int identity);
__global__ void prescan_MultiBlock(long long int *output, long long int *input, long long int n, long long int* sums, long long int operation,long long int identity);
__global__ void add_two(long long int *output, long long int length, long long int *n1);
__global__ void add_three(long long int *output, long long int length, long long int *n1, long long int *n2);
__global__ void max_two(long long int *output, long long int length, long long int *n1);
__global__ void max_three(long long int *output, long long int length, long long int *n1, long long int *n2);
__global__ void min_two(long long int *output, long long int length, long long int *n1);
__global__ void min_three(long long int *output, long long int length, long long int *n1, long long int *n2);
__global__ void exc_to_inc(long long output, long long input,long long int operations);

void _checkCudaError(const char *message, cudaError_t err, const char *caller);
void printResult(const char* prefix, long long int result, float milliseconds);
void printArrayInFile (const char* prefix ,long long int Output[], long long int start, long long int end);



void Scan(long long int N, long long int Option, long long int operation, long long int printing) {
	
	time_t t;
	srand((unsigned)time(&t));
	long long int *in =(long long int *) malloc (sizeof(long long int) * (N+1));	//input array
	long long int *mod_input =(long long int *) malloc (sizeof(long long int) * (N+1));// modified input array incase of Subtraction being the operation
	
	
	if(Option == 1 )		// IF THE ARRAY NEEDS TO BE RANDOMLY GENERATED
	{

		printf("Generating Random Numbers...\n");
		in[0] = rand()%1000000;
		mod_input[0] = in[0];
		
		for (long long int i = 1; i < N; i++) 
		{
			in[i] = rand() % 1000000;
			mod_input[i] = -in[i];	
		}
		if(operation==4)
		{
			in[N] = LONG_LONG_MAX;
		}
		else if (operation==3)
		{
			in[N] = LONG_LONG_MIN;
		}
		else
		{
			in[N] = 0;
			mod_input[N] = 0;	
		
		}

		printf("Finished Generating Random Numbers...\n\n");
	}
	else					// IF THE ARRAY IS FED AS INPUT TO THE PROGRAM
	{
		printf("Please type the desired %lld values of the vector each seperated by an ENTER KEY or WHITESPACE\n",N);
		scanf("%lld",&in[0]);
		mod_input[0] = in[0];
		for (long long int i = 1; i < N; i++) 
		{
			scanf("%lld",&in[i]);
			mod_input[i] = -in[i];
		}
		if(operation==4)
		{
			in[N] = LONG_LONG_MAX;
		}
		else if (operation==3)
		{
			in[N] = LONG_LONG_MIN;
		}
		else
		{
			in[N] = 0;
			mod_input[N] = 0;	
		
		}
		printf("Finished Taking Input...\n\n");
	
	}

	
	
	
	if(printing)		//PRINTING THE OUTPUT ARRAY TO output.txt
	{	
		printf("Printing the Input Vector...\n");
		FILE* fp = fopen("output.txt", "w");		
		printArrayInFile("Original Array",in, 0 , N);
		fclose(fp);
		printf("Finished Printing the Input Vector \n");
	}

	long long int *output_CPU = (long long int *) malloc (sizeof(long long int) * (N+1));
	printf("Doing the sequential Exclusive scan...\n");
	float time_host = sequential_scan(output_CPU, in, (N+1), operation);
	printf("Finished the sequential Exclusive scan...\n\n");
	
	//Printing The Result and Time
	printResult("Host Time  ", output_CPU[N], time_host);
	if(printing)		//PRINTING THE OUTPUT ARRAY TO output.txt
	{	
		printf("Printing the Scanned Vector formed by the CPU...\n");
		printArrayInFile ("HOST RESULT", output_CPU, 1, N+1);
		printf("Finished Printing the Scanned Vector formed by the CPU\n\n");
		
	}
	
	
	// Parallel scan on GPU
	printf("Doing the Parallel Exclusive scan...\n");
	long long int *output_GPU = (long long int *) malloc (sizeof(long long int) * N);
	printf("Finished the Parallel Exclusive scan...\n");
	
	if(operation == 2)		// Special consideration for subtraction because the operation is not associative
	{

		float time_gpu = scan(output_GPU, mod_input, N+1, operation);
		printResult("GPU time ", output_GPU[N], time_gpu);
	}
	else				// for all the other operationators
	{
		float time_gpu = scan(output_GPU, in, N + 1, operation);
		printResult("GPU time ", output_GPU[N], time_gpu);
	}
	if(printing)
	{
		printf("Printing the Scanned Vector formed by the GPU...\n");
		printArrayInFile ("GPU RESULT", output_GPU,1, N + 1);
		printf("Finished Printing the Scanned Vector formed by the GPU\n\n");
		printf("Please look at the output.txt to see the scanned vectors and input vector\n");
	}

	// For checking correctness of solution
	check(output_CPU,output_GPU,1,N + 1);

	//clean up of all memory used up
	free(in);
	free(mod_input);
	//free(output_CPU);
	free(output_GPU);
	}

int main(){
	long long int N=0, options=0, operation=0;
	char printing = 0;
	
	printf("Please input a proper size of the array or vector\n");
	scanf("%lld",&N);
	if(N <= 0)
	{
		printf("Please input a proper number which is greater than zero for the size\n");
		printf("The application would terminate now\n");
		return 0;
	}

	printf("Please select one of the given options \n");
	printf("\t1)Randomize the Elements input array of size %lld\n", N);
	printf("\t2)Proived the Elements of input array of size %lld\n", N);
	printf("Type 1 or 2 depending upon the option you want to select\n");
	scanf("%lld", &options);
	printf("\n");
	if(options!=1&&options!=2)
	{
		printf("Please type either 1 or 2 only next time for selecting the Options\n");
		printf("The application would terminate now\n");
		return 0;
	}

	printf("Please select one of the given operations\n");
	printf("1)Addition\t2)Subtraction\t3)Maximum\t4)Minimum\n");
	printf("Type 1,2,3 or 4 depending upon the operation you want to select\n");
	scanf("%lld", &operation);
	printf("\n");
	if(operation!=1 && operation!=2 && operation!=3 && operation!=4)
	{
		printf("Please type either 1,2,3 OR 4 only next time for selecting the Operator\n");
		printf("The application would terminate now\n");
		return 0;
	}
	getchar();	// to eat the enter key;
	
	printf("Do you wish to print the input and scanned vector in an output.txt file?\n");
	printf("Type y for Yes or n for No\n");
	scanf("%c", &printing);
	printf("\n");

	if(printing!='y'&& printing!='n')
	{
		printf("Please type either character 'y' OR 'n' only next time for choosing to print or not the vectors\n");
		printf("The application would terminate now\n");
		return 0;
	}

	if(printing=='y')
	{
		Scan(N,options,operation,1);
	}
	else
	{
		Scan(N,options,operation,0);
	}
	
	return 0;}

float sequential_scan(long long int* output, long long int* input, long long int length, long long int operation) {
	struct timeval start, end;
    gettimeofday(&start, NULL);
	switch(operation)
	{
		case 1:
		{
			output[0] = 0; // since this is an exclusive scan
			output[1] = input[0]; 
			for (long long int j = 2; j < length; ++j)
			{
				output[j] = input[j - 1] + output[j - 1];
			}
		break;
		}
		case 2:
		{
			output[0] = 0; // since this is an exclusive scan
			output[1] = input[0]; 
			for (long long int j = 2; j < length; ++j)
			{
				output[j] = output[j - 1] - input[j - 1];
			}
		break;
		}
		case 3:
		{
			output[0] = LONG_LONG_MIN;
			output[1] = input[0];
			for (long long int j = 2; j < length; ++j)
			{
				if(input[j-1] > output[j - 1])
					output[j] = input[j-1];
				else
					output[j] = output[j-1];
			}
		break;
		}
		case 4:
		{
			
			output[0] = LONG_LONG_MAX;	//since in exclusive scan the first element is the identity of the operator, in this case the max 								//number possible
			output[1] = input[0];
			for (long long int j = 2; j < length; ++j)
			{
				if(input[j-1] < output[j - 1])
					output[j] = input[j-1];
				else
					output[j] = output[j-1];
			}
		break;
		}
	}
	 
    gettimeofday(&end, NULL);
	float seconds = (end.tv_sec  - start.tv_sec);
    float micros = ((seconds * 1000000)+ (end.tv_usec - start.tv_usec));
	return (float)(micros/1000);}

float scan(long long int *output, long long int *input, long long int length,long long int operation) {
	
	long long int *device_input,*device_output;
	long long int arraySize = length * sizeof(long long int);

	cudaMalloc((void **)&device_output, arraySize);
	cudaMalloc((void **)&device_input, arraySize);
	cudaMemcpy(device_output, output, arraySize, cudaMemcpyHostToDevice);
	cudaMemcpy(device_input, input, arraySize, cudaMemcpyHostToDevice);

	cudaEvent_t initial, final;
	cudaEventCreate(&initial);
	cudaEventCreate(&final);
	// starting the timer given in CUDA Library
	cudaEventRecord(initial);
	long long int identity = 0;
	if(operation==4)
	{
		identity = LONG_LONG_MAX;
	}
	else if (operation==3)
	{
			identity = LONG_LONG_MIN;
	}
	if (length <= ELEMENTS_PER_BLOCK) 
	{
		/*float elapsed = */ scanSingleBlock(device_output, device_input, length, operation, identity);
		//printf("The parallelizable part took %lf ms of time", elapsed);//needed for finding parallelizable part
	}
	else 
	{
		/*float elpased  = */scanMultiBlock(device_output, device_input, length, operation , identity);
		//printf("The parallelizable part took %lf ms of time", elapsed);	//needed for finding parallelizable part
	}

	// end timer
	cudaEventRecord(final);
	cudaEventSynchronize(final);
	float elapsedTime = 0;
	cudaEventElapsedTime(&elapsedTime, initial, final);

	cudaMemcpy(output, device_output, arraySize, cudaMemcpyDeviceToHost);

	//clean up
	cudaFree(device_input);
	cudaFree(device_output);
	cudaEventDestroy(final);
	cudaEventDestroy(initial);
	

	return elapsedTime;}

/*float*/  //needed return type for finding parallelizable part
void scanMultiBlock(long long int *device_output, long long int *device_input, long long int length,long long int operation, long long int identity) {
	long long int reminder = length % (ELEMENTS_PER_BLOCK);
	
	//float temp_time =0, elapsed_time = 0; //needed for finding parallelizable part
	if (reminder != 0) 
	{
		// perform a large scan on a compatible multiple of elements
		long long int blockMultiple = length - reminder;
		/*elapsed_time += */scanBlockSizedArray(device_output, device_input, blockMultiple,  operation, identity); // needed varaible for finding parallelizable part

		// scan the remaining elements and add the (inclusive) last element of the large scan to this
		long long int *startOfOutputArray = &(device_output[blockMultiple]);
		long long int *startOfInputArray = &(device_input[blockMultiple]);
		/*elapsed_time +=*/ scanSingleBlock(startOfOutputArray, startOfInputArray, reminder,  operation, identity);// needed varaible for finding parallelizable part
		/*
		// needed for finding parallelizable part 
		cudaEvent_t initial, final;
		cudaEventCreate(&initial);
		cudaEventCreate(&final);
		// starting the timer given in CUDA Library
		cudaEventRecord(initial);
		*/
		switch(operation)
		{
			case 1:
			{
				add_three<<<1, reminder>>>(startOfOutputArray, reminder, &(device_input[blockMultiple - 1]), &(device_output[blockMultiple - 1]));
				break;
			}
			case 2:
			{
				add_three<<<1, reminder>>>(startOfOutputArray, reminder, &(device_input[blockMultiple - 1]), &(device_output[blockMultiple - 1]));
				break;
			}
			case 3:
			{
				max_three<<<1, reminder>>>(startOfOutputArray, reminder, &(device_input[blockMultiple - 1]), &(device_output[blockMultiple - 1]));
				break;
			}
			
			case 4:
			{
				min_three<<<1, reminder>>>(startOfOutputArray, reminder, &(device_input[blockMultiple - 1]), &(device_output[blockMultiple - 1]));
				break;
			}
		}
		/*
		Needed for finding parallelizable part
		cudaEventRecord(final);
		cudaEventSynchronize(final);
		cudaEventElapsedTime(&temp_time, initial, final);
		elapsed_time += temp_time
		cudaFree(device_input);
		cudaFree(device_output);
		cudaEventDestroy(final);
		cudaEventDestroy(initial);
		return elapsed_time;
		*/

	}
	else 
	{
		// Both the comments are needed for finding parallelizable part 
		/*float elapsed_time = */ scanBlockSizedArray(device_output, device_input, length,  operation, identity);	
		/*return elapsed_time;*/
	}
}

/*float*/
void scanSingleBlock(long long int *device_output, long long int *device_input, long long int length,long long int operation, long long int identity) {

		long long int nextPowerOfTwo = 1;
		while (nextPowerOfTwo < length) 
		{
			nextPowerOfTwo *= 2;
		}
		/*
		Needed for finding parallelizable part
		float elapsed_time = 0;
		cudaEvent_t initial, final;
		cudaEventCreate(&initial);
		cudaEventCreate(&final);
		// starting the timer given in CUDA Library
		cudaEventRecord(initial);
		*/
		prescan_SingleBlock<<<1, (length + 1) / 2, 2 * nextPowerOfTwo * sizeof(long long int)>>>(device_output, device_input, length, nextPowerOfTwo, operation, identity);	
		/*
		Needed for finding parallelizable part
		
		cudaEventRecord(final);
		cudaEventSynchronize(final);
		cudaEventElapsedTime(&elapsed_time, initial, final);
		cudaFree(device_input);
		cudaFree(device_output);
		cudaEventDestroy(final);
		cudaEventDestroy(initial);
		return elapsed_time;
		*/
}

/*float*/
void scanBlockSizedArray(long long int *device_output, long long int *device_input, long long int length, long long int operation, long long int identity) {
	long long int num_blocks = length / ELEMENTS_PER_BLOCK;
	long long int sharedMemBlockSize = ELEMENTS_PER_BLOCK * sizeof(long long int);

	long long int *device_blocks, *device_inputcr;
	cudaMalloc((void **)&device_blocks, num_blocks * sizeof(long long int));
	cudaMalloc((void **)&device_inputcr, num_blocks * sizeof(long long int));
	/*
	Needed for finding parallelizable part	
	float elapsed_time = 0, temp_time;
	cudaEvent_t initial, final;
	cudaEventCreate(&initial);
	cudaEventCreate(&final);
	// starting the timer given in CUDA Library
	cudaEventRecord(initial);
	*/
	prescan_MultiBlock<<<num_blocks, THREADS_PER_BLOCK, 2*sharedMemBlockSize>>>(device_output, device_input, ELEMENTS_PER_BLOCK, device_blocks, operation, identity);
	/*
	Needed for finding parallelizable part

	cudaEventRecord(final);
	cudaEventSynchronize(final);
	cudaEventElapsedTime(&elapsed_time, initial, final);
	cudaFree(device_input);
	cudaFree(device_output);
	cudaEventDestroy(final);
	cudaEventDestroy(initial);
	*/
	if ((num_blocks + 1) / 2 < THREADS_PER_BLOCK) 
	{
		//Needed for finding parallelizable part
		/*elapsed_time+=*/scanSingleBlock(device_inputcr, device_blocks, num_blocks,  operation, identity);
	}
	else 
	{
		// Needed for finding parallelizable part
		/*elapsed_time+= */scanMultiBlock(device_inputcr, device_blocks, num_blocks,  operation, identity);
	}
	/*
	Needed for finding parallelizable part
	cudaEventCreate(&initial);
	cudaEventCreate(&final);
	// starting the timer given in CUDA Library
	cudaEventRecord(initial);
	*/
	switch(operation)
		{
			case 1:
			{
				add_two<<<num_blocks, ELEMENTS_PER_BLOCK>>>(device_output, ELEMENTS_PER_BLOCK, device_inputcr);
				break;
			}
			case 2:
			{
				add_two<<<num_blocks, ELEMENTS_PER_BLOCK>>>(device_output, ELEMENTS_PER_BLOCK, device_inputcr);
				break;
			}
			case 3:
			{
				max_two<<<num_blocks, ELEMENTS_PER_BLOCK>>>(device_output, ELEMENTS_PER_BLOCK, device_inputcr);
				break;
			}
			
			case 4:
			{
				min_two<<<num_blocks, ELEMENTS_PER_BLOCK>>>(device_output, ELEMENTS_PER_BLOCK, device_inputcr);
				break;
			}
		}
	/*
	Needed for finding parallelizable part
	
	cudaEventRecord(final);
	cudaEventSynchronize(final);
	cudaEventElapsedTime(&temp_time, initial, final);
	cudaFree(device_input);
	cudaFree(device_output);
	cudaEventDestroy(final);
	cudaEventDestroy(initial);
	elapsed_time +=  temp_time
	cudaFree(device_inputcr);
	cudaFree(device_blocks);
	return elapsed_time;
	*/
	}

__global__ void prescan_SingleBlock(long long int *output, long long int *input, long long int n, long long int nextPowerOfTwo, long long int operation, long long int identity)
{
	extern __shared__ long long int temp[];
	long long int threadID = threadIdx.x;
	long long int offset = 1;

	long long int index1 = threadID;
	long long int index2 = threadID + (n / 2);
	long long int bankOffsetB = CONFLICT_FREE_OFFSET(index2);
	long long int bankOffsetA = CONFLICT_FREE_OFFSET(index1);

	if (threadID >= n) {
		temp[index1 + bankOffsetA] = 0;
		temp[index2 + bankOffsetB] = 0;
	}

	else 
	{
			temp[index1 + bankOffsetA] = input[index1];
			temp[index2 + bankOffsetB] = input[index2];
	}


	for (long long int d = nextPowerOfTwo/2; d > 0; d= d/2) // Do the reduction by building a operation(like sum) tree in place
	{
		__syncthreads();
		
		if (threadID < d)
		{
			long long int index1 = offset * (2 * threadID + 1) - 1;
			long long int index2 = offset * (2 * threadID + 2) - 1;
			index1 += CONFLICT_FREE_OFFSET(index1);
			index2 += CONFLICT_FREE_OFFSET(index2);
			switch(operation)
			{
				case 1:
				{
					temp[index2] += temp[index1];
					break;
				}
			
				case 2:
				{
					temp[index2] += temp[index1];
					break;
				}
				case 3:
				{
					if(temp[index2] < temp[index1])
						temp[index2] = temp[index1];
					break;
				}
				case 4:
				{
					if(temp[index2] > temp[index1])
						temp[index2] = temp[index1];
					break;
				}}
		}
		offset *= 2;
	}
	__syncthreads();
	
	if (threadID == 0) 
	{		/*
			//FOR DEBUGGIN PURPOSE PLEASE IGNORE
			printf("%s\n", "TEMP");
			for(long long int i=0; i < n; i++)
			{
				printf("%lld ",temp[i]);
			}
			printf("\n");
			*/
		if(operation!=4)
		{
				temp[nextPowerOfTwo - 1 + CONFLICT_FREE_OFFSET(nextPowerOfTwo - 1)] = 0; // clear the last element for exclusive scan
		}
		else
		{
				temp[nextPowerOfTwo - 1 + CONFLICT_FREE_OFFSET(nextPowerOfTwo - 1)] = identity; // clear the last element for exclusive scan
		}
	}

	for (long long int d = 1; d < nextPowerOfTwo; d *= 2) // traverse down tree & build scan
	{
		offset = offset/2;
		__syncthreads();
		if (threadID < d)
		{
			long long int index1 = offset * (2 * threadID + 1) - 1;
			long long int index2 = offset * (2 * threadID + 2) - 1;
			index1 += CONFLICT_FREE_OFFSET(index1);
			index2 += CONFLICT_FREE_OFFSET(index2);

			long long int t = temp[index1];
			temp[index1] = temp[index2];
			switch(operation)
			{
				case 1:
				{
					temp[index2] += t;
					break;
				}
				case 2:
				{
					temp[index2] += t;
					break;
				}
				case 3:
				{
					if(temp[index2] < t)
						temp[index2] = t;
					break;
				}
				case 4:
				{
					if(temp[index2] > t)
						temp[index2] = t;
					break;
				}}
		}
	}
	__syncthreads();

	if (threadID < n) 
	{
		output[index1] = temp[index1 + bankOffsetA];
		output[index2] = temp[index2 + bankOffsetB];
	}}

__global__ void prescan_MultiBlock(long long int *output, long long int *input, long long int n, long long int *sums,long long int operation,long long int identity) {
	extern __shared__ long long int temp[];

	long long int blockID = blockIdx.x;
	long long int threadID = threadIdx.x;
	long long int blockOffset = blockID * n;

	long long int index1 = threadID;
	long long int index2 = threadID + (n / 2);
	long long int bankOffsetA = CONFLICT_FREE_OFFSET(index1);
	long long int bankOffsetB = CONFLICT_FREE_OFFSET(index2);
	temp[index1 + bankOffsetA] = input[blockOffset + index1];
	temp[index2 + bankOffsetB] = input[blockOffset + index2];

	long long int offset = 1;
	for (long long int d = n >> 1; d > 0; d >>= 1) // build sum in place up the tree
	{
		__syncthreads();
		if (threadID < d)
		{
			long long int index1 = offset * (2 * threadID + 1) - 1;
			long long int index2 = offset * (2 * threadID + 2) - 1;
			index1 += CONFLICT_FREE_OFFSET(index1);
			index2 += CONFLICT_FREE_OFFSET(index2);
			switch(operation)
			{
				case 1:
				{
					temp[index2] += temp[index1];
					break;
				}
				case 2:
				{
					temp[index2] += temp[index1];
					break;
				}
				case 3:
				{
					if(temp[index2] < temp[index1])
						temp[index2] = temp[index1];
					break;
				}
				case 4:
				{
					if(temp[index2] > temp[index1])
						temp[index2] = temp[index1];
				}
			}
		}
		offset *= 2;
	}
	__syncthreads();


	if (threadID == 0) 
	{
		sums[blockID] = temp[n - 1 + CONFLICT_FREE_OFFSET(n - 1)];
		if(operation!=4)
		{
			temp[n - 1 + CONFLICT_FREE_OFFSET(n - 1)] = 0;
		}
		else
		{
			temp[n - 1 + CONFLICT_FREE_OFFSET(n - 1)] = identity;
		}
	}

	for (long long int d = 1; d < n; d *= 2) // traverse down tree & build scan
	{
		offset >>= 1;
		__syncthreads();
		if (threadID < d)
		{
			long long int index1 = offset * (2 * threadID + 1) - 1;
			long long int index2 = offset * (2 * threadID + 2) - 1;
			index1 += CONFLICT_FREE_OFFSET(index1);
			index2 += CONFLICT_FREE_OFFSET(index2);

			long long int t = temp[index1];
			temp[index1] = temp[index2];
			switch(operation)
			{
				case 1:
				{
					temp[index2] += t;
					break;
				}
				case 2:
				{
					temp[index2] += t;
					break;
				}
				case 3:
				{
					if(temp[index2] < t)
						temp[index2] = t;
					break;
				}
				case 4:
				{
					if(temp[index2] > t)
						temp[index2] = t;
					break;
				}
			}
		}
	}
	__syncthreads();

	output[blockOffset + index1] = temp[index1 + bankOffsetA];
	output[blockOffset + index2] = temp[index2 + bankOffsetB];
}


//functions to add two or three numbers in given arrays
__global__ void add_two(long long int *output, long long int length, long long int *n) {
	long long int blockID = blockIdx.x;
	long long int threadID = threadIdx.x;
	long long int blockOffset = blockID * length;

	output[blockOffset + threadID] += n[blockID];}
__global__ void add_three(long long int *output, long long int length, long long int *n1, long long int *n2) {
	long long int blockID = blockIdx.x;
	long long int threadID = threadIdx.x;
	long long int blockOffset = blockID * length;

	output[blockOffset + threadID] += n1[blockID] + n2[blockID];}

//functions to find maximum of two or three numbers in given arrays
__global__ void max_two(long long int *output, long long int length, long long int *n) {
	long long int blockID = blockIdx.x;
	long long int threadID = threadIdx.x;
	long long int blockOffset = blockID * length;
	if(output[blockOffset + threadID] < n[blockID])
	{
			output[blockOffset + threadID] = n[blockID];
	}}
__global__ void max_three(long long int *output, long long int length, long long int *n1, long long int *n2) {
	long long int blockID = blockIdx.x;
	long long int threadID = threadIdx.x;
	long long int blockOffset = blockID * length;
	if(n1[blockID] > n2[blockID])
	{
		if(n1[blockID] > output[blockOffset + threadID])
		{
			output[blockOffset + threadID] = n1[blockID];
		}
	}
	else
	{
		if(n2[blockID] > output[blockOffset + threadID])
		{
			output[blockOffset + threadID] = n2[blockID];
		}
	}}

//functions to find minimum of two or three numbers in given arrays
__global__ void min_two(long long int *output, long long int length, long long int *n) {
	long long int blockID = blockIdx.x;
	long long int threadID = threadIdx.x;
	long long int blockOffset = blockID * length;

	if(output[blockOffset + threadID] > n[blockID])
	{
			output[blockOffset + threadID] = n[blockID];
	}}
__global__ void min_three(long long int *output, long long int length, long long int *n1, long long int *n2) {
	long long int blockID = blockIdx.x;
	long long int threadID = threadIdx.x;
	long long int blockOffset = blockID * length;

	if(n1[blockID] < n2[blockID])
	{
		if(n1[blockID] < output[blockOffset + threadID])
		{
			output[blockOffset + threadID] = n1[blockID];
		}
	}
	else
	{
		if(n2[blockID] < output[blockOffset + threadID])
		{
			output[blockOffset + threadID] = n2[blockID];
		}
	}}

void _checkCudaError(const char *message, cudaError_t err, const char *caller) {
	if (err != cudaSuccess) {
		fprintf(stderr, "Error in: %s\n", caller);
		fprintf(stderr, "%s\n", message);
		fprintf(stderr, ": %s\n", cudaGetErrorString(err));
		exit(0);}}

void printResult(const char* Heading, long long int result, float milliseconds) {
	printf("%s\n", Heading);
	printf("Final Reduction is %lld and it was done in %lf ms\n", result, milliseconds);}

void printArrayInFile (const char* Header ,long long int Output[], long long int start, long long int end){
	FILE* fp = fopen("output.txt", "a");
	fprintf(fp, "%s\n", Header);
	for(long long int i=start; i < end; i++)
	{
		fprintf(fp, "%lld ",Output[i]);
	}
	fprintf(fp, "\n");
	fclose(fp);}

void check(long long int* CPU_Vector,long long int* GPU_Vector, long long int start, long long int end)
{
	for(int i=start; i < end; i++)
	{
		if(CPU_Vector[i]!=GPU_Vector[i])
		{
			printf("Outputs don't match\n");
			return;
		}
	}
	printf("Outputs do match, The implementation is successful\n");}
