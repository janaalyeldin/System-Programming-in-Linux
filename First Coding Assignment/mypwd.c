#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX 100
int main(){
	char cwd[MAX];
	getcwd(cwd,sizeof(cwd));
	printf("Working directory: %s\n",cwd);

}
