#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define COUNT 100

int main(int argc, char* argv[]){
	char buf[COUNT];
	if(argc<3){
        printf("Usage:  %s file-name\n" ,argv[0]);
       exit(-1); //that means im waiting for argv[0] esm el barnamg argv[1] dah el argument ely ha2rah
        }

	int fd=open(argv[1],O_RDONLY);
        if(fd<0){
                printf("coudlnt open the file\n");
                exit(-2);
       }

	
	int openFlags = O_CREAT | O_WRONLY | O_TRUNC;
	mode_t  filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
	int fd2 = open(argv[2] , openFlags, filePerms);

	int num_read;
	while((num_read=read(fd,buf,COUNT))>0){
	
		if(write(fd2,buf,num_read)!=num_read){
		
			printf("Write failed\n");
                exit(-3);

		}
	}

close(fd);
close(fd2);
}
