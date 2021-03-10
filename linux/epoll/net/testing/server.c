#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <netinet/if_ether.h>

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

typedef struct _ethhdr
{
    uint8_t eth_da[6];
    uint8_t eth_sa[6];
    uint16_t eth_type;
}ETHERNET_HEADER;

#define ETHERNET_HEADER_LENGTH 14

void analyseIP(IP_HEADER *ip);
void analyseTCP(TCP_HEADER *tcp);
void analyseUDP(UDP_HEADER *udp);
void analyseICMP(ICMP_HEADER *icmp);

void analyseETHERNET(ETHERNET_HEADER *ethernet);

int main(int argc, char** argv){
	int listenfd;
    IP_HEADER *ip = NULL;
    ETHERNET_HEADER *eth = NULL;

	uint8_t buff[4096];
	int n;

	if((listenfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1)
    {
		printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
		exit(0);
	}

	/* 
	 * no need to bind or listen or connect in SOCK_RAW
	 */

	while(1)
    {
		//printf("ready\n");
		n = recv(listenfd, buff, sizeof(buff), 0);
		if(n < 1)
			continue;

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

        eth = (ETHERNET_HEADER*)buff;
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

	close(listenfd);

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

    p = (uint8_t*)ethernet->eth_da;
    printf("destination address: %02X:%02X:%02X:%02X:%02X:%02X\n",p[0],p[1],p[2],p[3],p[4],p[5]);
    p = (uint8_t*)ethernet->eth_sa;
    printf("     source address: %02X:%02X:%02X:%02X:%02X:%02X\n",p[0],p[1],p[2],p[3],p[4],p[5]);
    p = NULL;
}