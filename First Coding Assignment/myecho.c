#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


int main(int argc, char* argv[]){

	int i;
	for(i=1;i<argc;i++){
	
		printf("%s  ",argv[i]);
	}
	printf("\n");
	return 0;
}
