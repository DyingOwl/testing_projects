#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>              //keyboard
#include <unistd.h>             //read, close
#include <sys/socket.h>         //socket, *
#include <sys/ioctl.h>          //ioctl, SIOCGIFINDEX
#include <sys/epoll.h>          //epoll, *
#include <linux/if_ether.h>     //ETH_P_ALL
#include <linux/if_packet.h>    //sockaddr_ll
#include <net/if.h>             //ifreq, IFNAMSIZ
#include <arpa/inet.h>          //uintx_t, htons, ntohs

#define MAXLINE 4096

typedef struct _ethhdr_mac
{
	uint8_t mac[6];
}ETHERNET_HEADER_MAC;

typedef struct _ethhdr
{
    ETHERNET_HEADER_MAC eth_da;
    ETHERNET_HEADER_MAC eth_sa;
    uint16_t eth_type;
    uint8_t following;
}ETHERNET_HEADER;

#define ETHERNET_HEADER_LENGTH 14
#define MAC_MASK 0x0000FFFFFFFFFFFF
#define MAX_POLL_SIZE 8192

#define MAC1 {0x00, 0x02, 0xA0, 0x00, 0x99, 0x43}
#define MAC2 {0x00, 0x02, 0xA0, 0x00, 0x99, 0x45}

#define HOST

void analyseETHERNET(ETHERNET_HEADER *ethernet, uint16_t msg_n_type);

int set_send_msg(uint8_t *buff, ETHERNET_HEADER_MAC mac_dista, ETHERNET_HEADER_MAC mac_local, uint8_t *type, uint8_t *msg);


int main(int argc, char** argv){
	int msg_len;
	int listenfd, keyboard;
	int activefds, totalfds, epollfd;
	int timeout;

	struct epoll_event ev, pevent[MAX_POLL_SIZE];
	struct sockaddr_ll addr;
	struct ifreq interface;

    ETHERNET_HEADER *eth = NULL;
	uint8_t buff[4096];
	uint8_t test_mesg[4096] = {"test mesg "};

	//mac and ifname msg
#ifdef HOST
	const ETHERNET_HEADER_MAC mac_local = {MAC1};
	const ETHERNET_HEADER_MAC mac_dista = {MAC2};
	const char if_name[] = {"enp3s0"};
#else
	const ETHERNET_HEADER_MAC mac_dista = {MAC1};
	const ETHERNET_HEADER_MAC mac_local = {MAC2};
	const char if_name[] = {"ens4"};
#endif
	const uint16_t msg_type = 0x1234;
	const uint16_t msg_n_type = htons(msg_type);

	/* 
	 * Create fds to read and write
	 */

	//keyboard for importing test mesg
	keyboard = open("/dev/tty", O_RDONLY|O_NONBLOCK);

	//listenfd for receiving test mesg
	if((listenfd = socket(PF_PACKET, SOCK_RAW, msg_n_type)) == -1)
	{
		printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
		exit(0);
	}

	/* 
	 * Bind a device
	 */

	/* get interface index */
	memset(&interface, 0, sizeof(interface));
	strncpy(interface.ifr_name, if_name, IFNAMSIZ);

	ioctl(listenfd, SIOCGIFINDEX, &interface);

	/* fill sockaddr_ll struct to prepare binding */
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = msg_n_type;
	addr.sll_ifindex =  interface.ifr_ifindex;

	/* bind socket to */
	if(bind(listenfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_ll)) < 0)
	{
		printf("bind to device %s error: %s(errno: %d)\n", if_name, strerror(errno), errno);
		exit(0);
	}

	/* 
	 * These are for epoll
	 */

	//create epoll, set with EPOLLIN mod, add listener to epoll events
	epollfd = epoll_create(MAX_POLL_SIZE);
	ev.events = EPOLLIN;
	ev.data.fd = listenfd;

	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) < 0)
	{
		printf("epoll set listener insertion error: %s(errno: %d) fd=%d\n", strerror(errno), errno, listenfd);
		exit(0);
	}
	else
	{
		printf("===== get ready for client's require =====\n");
	}

	printf("sending test ...\n");
	msg_len = set_send_msg(buff, mac_dista, mac_local, (uint8_t *)&msg_n_type, (uint8_t *)test_mesg);
	if(sendto(listenfd, buff, msg_len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_ll)) < 0)
	{
		printf("send to error: %s(errno: %d)\n", strerror(errno), errno);
	}

	//for the keyboard
	ev.events = EPOLLIN;
	ev.data.fd = keyboard;
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, keyboard, &ev) < 0)
	{
		printf("epoll set keyboard insertion error: %s(errno: %d) fd=%d\n", strerror(errno), errno, keyboard);
		exit(0);
	}
	else
	{
		printf("===== get ready for input =====\n");
	}

	/* 
	 * While(1)
	 */

	//default num of fds to poll
	totalfds = 2;
	//wait time limit
	timeout = 10000;

	//Ready for circling
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
			if(pevent[i].data.fd == listenfd)
			{
				//listened sth
				int n = recv(listenfd, buff, sizeof(buff), 0);
				if(n < 1)
					continue;
				
		        eth = (ETHERNET_HEADER *)buff;

		        //check source address
		        if(((*(uint64_t *)&(eth->eth_sa) ^ *(uint64_t *)&mac_dista) & MAC_MASK) == 0)
		        {
		        	printf("from me\n");
		        }

				printf("recv msg : \n");
				int i = 0;
				//addr and type msg
		        for( ; i < ETHERNET_HEADER_LENGTH; ++i)
		        {
            		printf("%02X ", buff[i]);
            		if((i % 6) == 5)
            		{
                		printf("   ");
            		}
        		}
        		printf("\n");

        		//string msg
		        for( ; i < n; ++i)
		        {
            		printf("%02X ", buff[i]);
            		if((i % 8) == 5)
            		{
                		printf("   ");
            		}
            		if((i % 16) == 13)
            		{
                		printf("\n");
            		}
        		}
        		printf("\n");

		        printf("analysing MAC msg\n");
		        //msg in form
		        analyseETHERNET(eth, msg_n_type);
				
        		printf("\n\n");
			}
			else if(pevent[i].events & EPOLLIN)
			{
				//require to send
				int n = read(keyboard, test_mesg, sizeof(test_mesg));
				if(n < 1)
					continue;
				//remove '\n'
				test_mesg[n - 1] = '\0';

				//print back sending msg
				printf("sending :\n%s\n", test_mesg);
				//push msg packge
				msg_len = set_send_msg(buff, mac_dista, mac_local, (uint8_t *)&msg_n_type, (uint8_t *)test_mesg);
				//sending
				if(sendto(listenfd, buff, msg_len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_ll)) < 0)
				{
					printf("send to error: %s(errno: %d)\n", strerror(errno), errno);
				}
			}
		}
	}

	close(listenfd);
	close(keyboard);
	close(epollfd);

	return 0;
}

void analyseETHERNET(ETHERNET_HEADER *ethernet, uint16_t msg_n_type)
{
    uint8_t* p = NULL;
    printf("ETHERNET -----\n");
    printf("type: %04X\n", ntohs(ethernet->eth_type));

    p = (uint8_t*)&ethernet->eth_da;
    printf("destination address: %02X:%02X:%02X:%02X:%02X:%02X\n",p[0],p[1],p[2],p[3],p[4],p[5]);
    p = (uint8_t*)&ethernet->eth_sa;
    printf("     source address: %02X:%02X:%02X:%02X:%02X:%02X\n",p[0],p[1],p[2],p[3],p[4],p[5]);
    if(ethernet->eth_type == msg_n_type)
    {
    	p = (uint8_t*)&ethernet->following;
    	printf("get msg: %s\n", p);
    }
    return;
}

int set_send_msg(uint8_t *buff, ETHERNET_HEADER_MAC mac_dista, ETHERNET_HEADER_MAC mac_local, uint8_t *type, uint8_t *msg)
{
	int len = 14, i;

	//destination address
	for(i = 0; i < 6; ++i)
	{
		*buff = mac_dista.mac[i];
		++buff;
	}

	//source address
	for(i = 0; i < 6; ++i)
	{
		*buff = mac_local.mac[i];
		++buff;
	}

	//type
	*buff = type[0];
	++buff;
	*buff = type[1];
	++buff;

	//msg
	for(i = 1; *msg != '\0'; ++i)
	{
		*buff = *msg;
		++buff;
		++msg;
	}
	*buff = '\0';

	return len + i;
}