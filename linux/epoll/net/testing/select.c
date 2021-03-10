#include <sys/time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#define MAX_EVENTS 10

int main(){
	int keyboard;
	int ret, i;
	char c;
	fd_set readfd;
	struct timeval timeout;

	int conn_sock, listen_sock;

	keyboard = open("/dev/tty", O_RDONLY|O_NONBLOCK);
	assert(keyboard > 0);

	printf("In select model...\n");
	while(1){
		printf("mark\n");
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		FD_ZERO(&readfd);
		FD_SET(keyboard, &readfd);
		ret = select(keyboard + 1, &readfd, NULL, NULL, &timeout);
		if(FD_ISSET(keyboard, &readfd)){
			i = read(keyboard, &c, 1);
			if('\n' == c)
				continue;
			printf("input is %c\n", c);

			if('q' == c)
				break;
		}


	}

	return 0;
}