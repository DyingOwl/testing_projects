#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAXLINE 4096

int exit_cmp(char* buff);

int main(int argc, char** argv){
	int sockfd, n;
	char recvline[4096], sendline[4096];
	struct sockaddr_in servaddr;

	if(argc != 2){
		printf("usage: ./client <ipaddress>\n");
		exit(0);
	}

	if((sockfd = socket(PF_PACKET, SOCK_RAW, 0) < 0){
		printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
		exit(0);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(6666);
	if(inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0){
		printf("inet_pton error for %s\n", argv[1]);
		exit(0);
	}

	printf("send msg to server: \n");
	while(fgets(sendline, 4096, stdin)){
		if(send(sockfd, sendline, strlen(sendline), 0) < 0){
			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
			exit(0);
		}
		if(exit_cmp(sendline))
			break;
	}


	close(sockfd);

	return 0;
}

int exit_cmp(char* buff){
	if(buff[0] == 'e')
		return 1;
	return 0;
}