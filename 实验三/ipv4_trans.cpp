#include "sysInclude.h"
#include <stdio.h>
#include <vector>
#include <vector>
using namespace std;
// system support
extern void fwd_LocalRcv(char* pBuffer, int length);

extern void fwd_SendtoLower(char* pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char* pBuffer, int type);

extern unsigned int getIpv4Address();

// implemented by students

vector<stud_route_msg> routeTable;

void stud_Route_Init()
{
	//已作为全局变量创建
	return;
}

void stud_route_add(stud_route_msg* proute)
{
	stud_route_msg route;
	unsigned int dest = ntohl(proute->dest);
	unsigned int masklen = ntohl(proute->masklen);
	unsigned int nexthop = ntohl(proute->nexthop);
	route.dest = dest;
	route.masklen = masklen;
	route.nexthop = nexthop;
    routeTable.push_back(route);
	return;
}

int stud_fwd_deal(char* pBuffer, int length)
{
    //读取字段内容，大于一个字节需要更改字节序
    char version = pBuffer[0] >> 4; //4bit版本号
    char IHL = pBuffer[0] & 0x0f;   //4bit首部长度
    char TTL = (char)pBuffer[8];    //8bit寿命
    short checksum = ntohs(*(unsigned short*)(pBuffer + 10));   //16bit校验和
    int destination = ntohl(*(unsigned int*)(pBuffer + 16));    //32bit目的IP地址

    //检验TTL
    if (TTL <= 0) {
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
        return 1;
    }
    //目的地址位为本机
    if (destination == getIpv4Address())
    {
        fwd_LocalRcv(pBuffer, length);
        return 0;
    }

    //查找路由表
    vector<stud_route_msg>::iterator it;
    for (it = routeTable.begin(); it != routeTable.end(); it++) {
        unsigned int mask = (1 << 31) >> (it->masklen - 1);
        unsigned int pre = it->dest & mask;
        if (destination == pre) {
            break;
        }
    }
    if (it == routeTable.end()) {//找不到路由
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
        return 1;
    }
    else {//转发处理
        //复制原数据报
        char* buffer = (char*)malloc(length);
        memcpy(buffer, pBuffer, length);
        //修改各字段
        buffer[8] = TTL - 1;
        memset(buffer + 10, 0, 2);
        //计算校验和
		unsigned long cksum = 0;
		for (int i = 0; i < 20; i += 2) {//头部每2字节相加
			unsigned short sum = ntohs(*((unsigned short*)(buffer + i)));
			cksum += (unsigned long)sum;
		}
		//printf("1:%x\n",cksum);
		cksum = (cksum >> 16) + (cksum & 0xffff);//将高16bit与低16bit相加
		cksum += cksum >> 16;//取进位再与低16bit相加
		unsigned short checksum = (unsigned short)(cksum);//取低16bit
		checksum = ~checksum;
		checksum = htons(checksum);
		memcpy(buffer + 10, &checksum, 2);
		fwd_SendtoLower(buffer, length, it->nexthop);
	}
    return 0;
}
