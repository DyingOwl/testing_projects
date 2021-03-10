#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAXLINE 4096

typedef struct _iphdr //定义IP首部 
{ 
    uint8_t h_verlen; //4位首部长度+4位IP版本号 
    uint8_t tos; //8位服务类型TOS 
    uint16_t total_len; //16位总长度（字节） 
    uint16_t ident; //16位标识 
    uint16_t frag_and_flags; //3位标志位 
    uint8_t ttl; //8位生存时间 TTL 
    uint8_t proto; //8位协议 (TCP, UDP 或其他) 
    uint16_t checksum; //16位IP首部校验和 
    uint32_t sourceIP; //32位源IP地址 
    uint32_t destIP; //32位目的IP地址 
}IP_HEADER; 

typedef struct _udphdr //定义UDP首部
{
    uint16_t uh_sport;    //16位源端口
    uint16_t uh_dport;    //16位目的端口
    uint32_t uh_len;//16位UDP包长度
    uint32_t uh_sum;//16位校验和
}UDP_HEADER;

typedef struct _tcphdr //定义TCP首部 
{ 
    uint16_t th_sport; //16位源端口 
    uint16_t th_dport; //16位目的端口 
    uint32_t th_seq; //32位序列号 
    uint32_t th_ack; //32位确认号 
    uint8_t th_lenres;//4位首部长度/6位保留字 
    uint8_t th_flag; //6位标志位
    uint16_t th_win; //16位窗口大小
    uint16_t th_sum; //16位校验和
    uint16_t th_urp; //16位紧急数据偏移量
}TCP_HEADER; 

typedef struct _icmphdr 
{  
    uint8_t  icmp_type;  
    uint8_t icmp_code; /* type sub code */  
    uint16_t icmp_cksum;  
    uint16_t icmp_id;  
    uint16_t icmp_seq;  
    /* This is not the std header, but we reserve space for time */  
    uint16_t icmp_timestamp;  
}ICMP_HEADER;

typedef struct _ethhdr_mac
{
	uint8_t mac[6];
}ETHERNET_HEADER_MAC;

typedef struct _ethhdr
{
    ETHERNET_HEADER_MAC eth_da;
    ETHERNET_HEADER_MAC eth_sa;
    uint16_t eth_type;
}ETHERNET_HEADER;

#define ETHERNET_HEADER_LENGTH 14
#define MAC_MASK 0x0000FFFFFFFFFFFF

void analyseIP(IP_HEADER *ip);
void analyseTCP(TCP_HEADER *tcp);
void analyseUDP(UDP_HEADER *udp);
void analyseICMP(ICMP_HEADER *icmp);

void analyseETHERNET(ETHERNET_HEADER *ethernet);


/* 
 * These are new for epoll (0)
 */
#include <sys/epoll.h>

#define MAX_POLL_SIZE 8192
/* 
 * (0) end...
 */

int main(int argc, char** argv){
	int listenfd;
    IP_HEADER *ip = NULL;
    ETHERNET_HEADER *eth = NULL;

	uint8_t buff[4096];
	int n;

	ETHERNET_HEADER_MAC mac_local;
	mac_local.mac[0] = 0x00;
	mac_local.mac[1] = 0x02;
	mac_local.mac[2] = 0xA0;
	mac_local.mac[3] = 0x00;
	mac_local.mac[4] = 0xA4;
	mac_local.mac[5] = 0xA2;

	struct sockaddr_ll addr;
	struct ifreq interface;
	const char opt[] = {"enp3s0"};

	/* 
	 * These are new for epoll (1)
	 */
	int activefds, totalfds, epollfd;
	int timeout, i;
	struct epoll_event ev, pevent[MAX_POLL_SIZE];

	/* 
	 * (1) end...
	 */

	if((listenfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1)
	{
		printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
		exit(0);
	}

	/* 
	 * bind a device
	 */

	/* get interface index */
	memset(&interface, 0, sizeof(interface));
	strncpy(interface.ifr_name, opt, IFNAMSIZ);

	ioctl(listenfd, SIOCGIFINDEX, &interface);

	/* fill sockaddr_ll struct to prepare binding */
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ALL);
	addr.sll_ifindex =  interface.ifr_ifindex;

	/* bind socket to */
	if(bind(listenfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_ll)) < 0)
	{
		printf("bind to device %s error: %s(errno: %d)\n", opt, strerror(errno), errno);
		exit(0);
	}

	/*old type
	if(setsockopt(listenfd, SOL_SOCKET, SO_BINDTODEVICE, &interface, sizeof(interface)) < 0)
	{
		printf("bind to device %s error: %s(errno: %d)\n", opt, strerror(errno), errno);
		exit(0);
	}
	*/

	/* 
	 * These are new for epoll (2)
	 */
	//create epoll, set with ET mod, add listener to epoll events
	epollfd = epoll_create(MAX_POLL_SIZE);
	ev.events = EPOLLIN;
	ev.data.fd = listenfd;

	/* 
	 * And with some changes for listener
	 */
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) < 0)
	{
		printf("epoll set listener insertion error: %s(errno: %d) fd=%d\n", strerror(errno), errno, listenfd);
		exit(0);
	}
	else
	{
		printf("===== get ready for client's require =====\n");
	}

	/* 
	 * And the new circling
	 */
	totalfds = 1;
	timeout = 60000;

	while(1)
	{
		activefds = epoll_wait(epollfd, pevent, totalfds, timeout);
		if(activefds == -1)
		{
			printf("epoll_wait error: %s(errno: %d)\n", strerror(errno), errno);
			break;
		}
		else if(activefds == 0)
		{
			printf("waiting timeout...\n");
			if(totalfds == 1)
				break;
		}

		for(i = 0; i < activefds; ++i)
		{
			if(pevent[i].events & (EPOLLERR | EPOLLHUP))
			{
				//error or hang up
				printf("connect lose...\n");
				close(pevent[i].data.fd);
				epoll_ctl(epollfd, EPOLL_CTL_DEL, pevent[i].data.fd, NULL);
				--totalfds;
				continue;

			}
			else if(pevent[i].data.fd == listenfd)
			{
				//listened sth
				n = recv(listenfd, buff, sizeof(buff), 0);
				if(n < 1)
					continue;
				
		        eth = (ETHERNET_HEADER*)buff;
		        /*
		        if((*(uint64_t*)&(eth->eth_da) ^ *(uint64_t*)&mac_local) & MAC_MASK)
		        {
		        	printf("not for this, ignore...\n");
		        	continue;
		        }
				*/
				printf("recv msg from client: \n");
		        for(int i = 0; i < n; ++i)
		        {
            		printf("%02X ", buff[i]);
            		if((i % 8) == 7)
            		{
                		printf(" ");
            		}
            		if((i % 16) == 15)
            		{
                		printf("\n");
            		}
        		}
        		printf("\n");

		        printf("analysing MAC msg\n");
		
		        analyseETHERNET(eth);
		        if(eth->eth_type == 0x0008)
		        {
            		printf("in ip mod\n");
            		ip = ( IP_HEADER *)(buff + ETHERNET_HEADER_LENGTH);
            		analyseIP(ip);
        		}
        		else
        		{
            		printf("no ip mod\n");
        		}
				
        		printf("\n\n");
			}
		}
	}

	/* 
	 * (2) end...
	 */

	close(listenfd);
	close(epollfd);

	return 0;

}
void analyseIP(IP_HEADER *ip)
{
    uint8_t* p = (uint8_t*)&ip->sourceIP;
    printf("Source IP\t: %u.%u.%u.%u\n",p[0],p[1],p[2],p[3]);
    p = (uint8_t*)&ip->destIP;
    printf("Destination IP\t: %u.%u.%u.%u\n",p[0],p[1],p[2],p[3]);

}

void analyseTCP(TCP_HEADER *tcp)
{
    printf("TCP -----\n");
    printf("Source port: %u\n", ntohs(tcp->th_sport));
    printf("Dest port: %u\n", ntohs(tcp->th_dport));
}

void analyseUDP(UDP_HEADER *udp)
{
    printf("UDP -----\n");
    printf("Source port: %u\n", ntohs(udp->uh_sport));
    printf("Dest port: %u\n", ntohs(udp->uh_dport));
}

void analyseICMP(ICMP_HEADER *icmp)
{
    printf("ICMP -----\n");
    printf("type: %u\n", icmp->icmp_type);
    printf("sub code: %u\n", icmp->icmp_code);
}

void analyseETHERNET(ETHERNET_HEADER *ethernet)
{
    uint8_t* p = NULL;
    printf("ETHERNET -----\n");
    printf("type: %04X\n", ethernet->eth_type);

    p = (uint8_t*)&ethernet->eth_da;
    printf("destination address: %02X:%02X:%02X:%02X:%02X:%02X\n",p[0],p[1],p[2],p[3],p[4],p[5]);
    p = (uint8_t*)&ethernet->eth_sa;
    printf("     source address: %02X:%02X:%02X:%02X:%02X:%02X\n",p[0],p[1],p[2],p[3],p[4],p[5]);
    p = NULL;
}

		/* 
		 * P.S.
		 * about(1) int epoll_create(int size);
		 * 
		 * size:    ignored after Linux 2.6.8, but must be set with a
		 *          positive value.
		 * return:  return a File Descriptor with nonnegative values.
		 *          return -1 if failed.
		 * 
		 * 
		 * about(2) int epoll_ctl(int epfd, int op, int fd, 
		 *                        struct epoll_event *event);
		 * 
		 * epfd:    shoule link to the fd(named epollfd here) created
		 *          by epoll_create.
		 * op:      have three types of values:
		 *          EPOLL_CTL_ADD,    add other fd to epollfd;
		 *          EPOLL_CTL_MOD, modify other fd to epollfd;
		 *          EPOLL_CTL_DEL, delete other fd to epollfd;
		 *          when set as DEL, *event can be ignored(set as NULL)
		 * fd:      the fd to be ADD|MOD|DEL here.
		 * *event:  an address of struct epoll_event, will be introduced
		 *          later.
		 * return:  return  0 if succeeded.
		 *          return -1 if failed.
		 *          errno contains more msg.
		 * 
		 * 
		 * about(3) struct epoll_event {
		 *            uint32_t     events; // Epoll events.
		 *            epoll_data_t data;   // User data variable.
		 *          };
		 * 
		 * events:  can set as following values:
		 *          EPOLLIN
		 *          The associated file is available for read.
		 *          EPOLLOUT
		 *          The associated file is available for write.
		 *          EPOLLRDHUP
		 *          Stream socket peer closed connection, or shut down
		 *          writing half of connection. (This flag is especially
		 *          useful for writing simple code to detect peer
		 *          shutdown when using Edge Triggered monitoring.)
		 *          EPOLLPRI
		 *          There is urgent data available for read operations.
		 *          EPOLLEER
		 *          Error condition happened on the associated file
		 *          descriptor. epoll_wait will always wait for this
		 *          event; it is not necessary to set it in events.
		 *          EPOLLHUP
		 *          Hang up happened on the associated file descriiptor.
		 *          epoll_wait will always wait for this event; it is
		 *          not necessary to set it in events.
		 *          EPOLLET
		 *          Sets the Edge Triggered behavior for the associated
		 *          file descriptor. The default behavior for epoll is
		 *          Level Triggered.
		 *          EPOLLONESHOT
		 *          Sets the one-shot behavior for the associated file
		 *          descriptor. This means that after an event is pulled
		 *          out with epoll_wait, the associated file descriptor
		 *          is internally disabled and no other events will be
		 *          reported by the epoll interface. The user must call
		 *          epoll_ctl() with EPOLL_CTL_MOD to re-enable the file
		 *          descriptor with a new event mask.
		 * 
		 * 
		 * about(4) typedef union epoll_data{
		 *            viod    *ptr;
		 *            int      fd;
		 *            uint32_t u32;
		 *            uint64_t u64;
		 *          }epoll_data_t;
		 * 
		 * 
		 * about(5) int epoll_wait(int epfd, struct epoll_event *events,
		 *                         int maxevents, int timeout);
		 * 
		 * epfd:    shoule link to the fd(named epollfd here) created
		 *          by epoll_create.
		 * *events:  a set of struct epoll_event, for returning happened
		 *          events. SHOULD NOT BE NULL.
		 * maxe...: the max num of events.
		 * timeout: waiting time(ms). 0: now; -1: fowever; else: ...
		 * return:  num of events should be dealt with.
		 *          return  0 if timeout.
		 *          return -1 if failed.
		 *          errno contains more msg.
		 */


