#include <math.h>
#include <iostream>


#define N 4

using namespace std;

int main(int argc , char* argv[])
{

	int A[N], B[N];
	
	#pragma omp parallel for num_threads(3)
	for(int i = 0 ; i < 10 ; i++)
	{
		for(;;)
		{
			A[i] = 2 * B[i];			
		}
	}
}
