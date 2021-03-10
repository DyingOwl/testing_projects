#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>              //keyboard
#include <unistd.h>             //read, close
#include <termios.h>            //tty
#include <sys/epoll.h>          //epoll, *

#define DEVICE_PATH "/dev/"
#define DEVICE_NAME_LEN 16

#define MAX_POLL_SIZE 8192

#define TEST_MESG "test mesg"
#define MESG_MAX_LEN 64

int init_tty(int fd, char *opt);
int find_word(char *buff, char word);

int main(int argc, char** argv){
	int ttyfd, keyboard;

	int activefds, epollfd;
	int totalfds = 0;
	int timeout;
	struct epoll_event ev, pevent[MAX_POLL_SIZE];

	char mesg[MESG_MAX_LEN];

	char tty_dev[DEVICE_NAME_LEN] = DEVICE_PATH;

	/* 
	 * Ready for environment
	 */

	//keyboard for control
	if((keyboard = open("/dev/tty", O_RDONLY|O_NONBLOCK)) <= 0)
	{
		printf("connect tty error: %s(errno: %d)\n", strerror(errno), errno);
		goto LEVEL_0;
	}
	else
	{
		printf("keyboard %d ready.\n", keyboard);
	}

	//create epoll, set with EPOLLIN mod, add listener to epoll events
	epollfd = epoll_create(MAX_POLL_SIZE);
	ev.events = EPOLLIN;
	ev.data.fd = keyboard;

	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, keyboard, &ev) < 0)
	{
		printf("epoll set listener insertion error: %s(errno: %d) fd=%d\n", \
			strerror(errno), errno, keyboard);
		goto LEVEL_1;
	}
	else
	{
		printf("epollfd %d with %d ready.\n", epollfd, keyboard);
		++totalfds;
	}

	timeout = 1000;

	/* 
	 * Test for input values
	 */
	if(argc < 3)
	{
		printf("losing values, set like \"ttySn 1152008n1\"\n");
		goto LEVEL_2;
	}

	strcat(tty_dev, argv[1]);

	printf("ready for tty: %s\n", tty_dev);

	/* 
	 * Create fds to read and write
	 */

	//read from this console
	if((ttyfd = open(tty_dev, O_RDWR)) <= 0)
	{
		printf("connect %s error: %s(errno: %d)\n", tty_dev, strerror(errno), errno);
		goto LEVEL_2;
	}
	else
	{
		printf("open %s to fd %d success.\n", tty_dev, ttyfd);
	}

	//set options
	if(init_tty(ttyfd, argv[2]) < 0)
	{
		printf("set %s with options %s error: %s(errno: %d)\n", \
			tty_dev, argv[2], strerror(errno), errno);
		goto LEVEL_3;
	}
	else
	{
		printf("%s set as %s success.\n", tty_dev, argv[2]);
	}

	//add to epoll
	ev.events = EPOLLIN;
	ev.data.fd = ttyfd;

	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, ttyfd, &ev) < 0)
	{
		printf("epoll set listener insertion error: %s(errno: %d) fd=%d\n", \
			strerror(errno), errno, ttyfd);
		goto LEVEL_3;
	}
	else
	{
		printf("epollfd %d with %d ready.\n", epollfd, ttyfd);
		++totalfds;
	}

	timeout = 60000;

	/* 
	 * While(1), 'q' to quiet
	 */

	printf("ready for connect.\n\n");
	while(1)
	{
		//wait for events
		activefds = epoll_wait(epollfd, pevent, totalfds, timeout);
		if(activefds == -1)
		{
			//error
			printf("epoll_wait error: %s(errno: %d)\n", strerror(errno), errno);
			break;
		}
		else if(activefds == 0)
		{
			//no event
			printf("waiting timeout...\n");
		}

		for(int i = 0; i < activefds; ++i)
		{
			//printf("%d activefds\n", activefds);
			if(pevent[i].data.fd == ttyfd)
			{
				if(ev.events == EPOLLIN)
				{
					//receive from the other hand
					int n = read(ttyfd, mesg, sizeof(mesg));
					if(n < 1)
						continue;
					--n;
					mesg[n] = '\0';

					printf("receive %d mesg from %s:\n%s\n", n, tty_dev, mesg);
					tcflush(ttyfd, TCIOFLUSH);
				}
				else if(ev.events == EPOLLOUT)
				{
					int n = write(ttyfd, mesg, strlen(mesg));
					
        			printf("sended %d words to %s\n\n", n, tty_dev);
					//tcflush(ttyfd, TCIFLUSH);

					ev.events = EPOLLIN;
					ev.data.fd = ttyfd;

					epoll_ctl(epollfd, EPOLL_CTL_MOD, ttyfd, &ev);
				}
			}
			else if(pevent[i].data.fd == keyboard)
			{
				//require to send
				int n = read(keyboard, mesg, sizeof(mesg));
				if(n < 1)
					continue;
				//should not remove '\n'
				mesg[n] = '\0';

				if(find_word(mesg, 'q') == 0)
					goto LEVEL_ALL;

				//print back sending msg
				printf("ready to send %d words:\n%s\n", n, mesg);

				ev.events = EPOLLOUT;
				ev.data.fd = ttyfd;

				epoll_ctl(epollfd, EPOLL_CTL_MOD, ttyfd, &ev);
			}
		}
	}

	/* 
	 * (2) end...
	 */

LEVEL_ALL:
LEVEL_3:
	close(ttyfd);
LEVEL_2:
	close(epollfd);
LEVEL_1:
	close(keyboard);
LEVEL_0:

	return 0;
}

#define SPEED_TYPES 8
//defined in termbits.h, which has included in termios.h
int speed_arr[SPEED_TYPES] = \
{B1200, B4800, B9600, B19200, B38400, B57600, B115200, B1500000};
unsigned int name_arr[SPEED_TYPES] = \
{ 1200,  4800,  9600,  19200,  38400,  57600,  115200,  1500000};

int init_tty(int fd, char *opt)
{
	unsigned int speed = 0;
	struct termios tms;

	char *ptr = opt;
	int i;

	tcgetattr(fd, &tms);
	//flush io queue
	tcflush(fd, TCIOFLUSH);

	//set speed
	while((*ptr >= '1') && (*ptr <= '9'))
	{
		speed = speed * 10 + (unsigned int)(*ptr - '0');
		++ptr;
	}
	while(*ptr == '0')
	{
		speed *= 10;
		++ptr;
	}


	for(i = 0; i < SPEED_TYPES; ++i)
	{
		if(speed == name_arr[i])
		{
			cfsetispeed(&tms, speed_arr[i]);
			cfsetospeed(&tms, speed_arr[i]);
			break;
		}
	}

	if(i == SPEED_TYPES)
	{
		printf("unknown speed type of %u\n", speed);
		return -(errno = 1);
	}

	//set databits
	tms.c_cflag &= ~CSIZE;

	switch(*ptr)
	{
	case '7':
		tms.c_cflag |= CS7;
		break;
	case '8':
		tms.c_cflag |= CS8;
		break;
	default:
		printf("unknown databits type of %c\n", *ptr);
		return -(errno = 1);
	}
	++ptr;
	
	//set parity
	switch(*ptr)
	{
	case 'n':
		tms.c_cflag &= ~PARENB; /* Clear parity enable */
		tms.c_iflag &= ~INPCK;  /* Enable parity checking */
		break;
	case 'e':
		tms.c_cflag |=  PARENB; /* Set parity enable */
		tms.c_cflag &= ~PARODD; /* Set as even */
		tms.c_iflag |=  INPCK;  /* Disable parity checking */
		break;
	case 'o':
		tms.c_cflag |=  PARENB; /* Set parity enable */
		tms.c_cflag |=  PARODD; /* Set as odd */
		tms.c_iflag |=  INPCK;  /* Disable parity checking */
		break;
	case 's': /* as no parity */
		tms.c_cflag &= ~PARENB;
		tms.c_cflag &= ~CSTOPB;
		tms.c_iflag |=  INPCK;
		break;
	default:
		printf("unknown parity type of %c\n", *ptr);
		return -(errno = 1);
	}
	++ptr;

	//set stopbits
	switch(*ptr)
	{
	case '1':
		tms.c_cflag &= ~CSTOPB;
		break;
	case '2':
		tms.c_cflag |=  CSTOPB;
		break;
	default:
		printf("unknown stopbits type of %c\n", *ptr);
		return -(errno = 1);
	}

	//set input parity option
	tms.c_cc[VMIN] = 0; /* minum bytes waiting for one read */
	tms.c_cc[VTIME] = 150; /* set waiting timeout as 15s */

	//use raw mode
	/*!!!ECHO must be disabled!!!*/
	tms.c_lflag &= ~(ICANON|ECHO|ECHOE|ISIG); /* Input */
	tms.c_oflag &= ~OPOST; /* Output */

	tcsetattr(fd, TCSANOW, &tms);
	tcflush(fd, TCIOFLUSH);

	return 0;
}

int find_word(char *buff, char word)
{
	int ret = 0;
	char *ptr = buff;
	while(*ptr != '\0')
	{
		if(*ptr == word)
		{
			break;
		}
		++ptr;
		++ret;
	}
	return ret;
}
