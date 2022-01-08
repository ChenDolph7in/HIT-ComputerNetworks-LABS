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
	//����Ϊȫ�ֱ�������
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
    //��ȡ�ֶ����ݣ�����һ���ֽ���Ҫ�����ֽ���
    char version = pBuffer[0] >> 4; //4bit�汾��
    char IHL = pBuffer[0] & 0x0f;   //4bit�ײ�����
    char TTL = (char)pBuffer[8];    //8bit����
    short checksum = ntohs(*(unsigned short*)(pBuffer + 10));   //16bitУ���
    int destination = ntohl(*(unsigned int*)(pBuffer + 16));    //32bitĿ��IP��ַ

    //����TTL
    if (TTL <= 0) {
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
        return 1;
    }
    //Ŀ�ĵ�ַλΪ����
    if (destination == getIpv4Address())
    {
        fwd_LocalRcv(pBuffer, length);
        return 0;
    }

    //����·�ɱ�
    vector<stud_route_msg>::iterator it;
    for (it = routeTable.begin(); it != routeTable.end(); it++) {
        unsigned int mask = (1 << 31) >> (it->masklen - 1);
        unsigned int pre = it->dest & mask;
        if (destination == pre) {
            break;
        }
    }
    if (it == routeTable.end()) {//�Ҳ���·��
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
        return 1;
    }
    else {//ת������
        //����ԭ���ݱ�
        char* buffer = (char*)malloc(length);
        memcpy(buffer, pBuffer, length);
        //�޸ĸ��ֶ�
        buffer[8] = TTL - 1;
        memset(buffer + 10, 0, 2);
        //����У���
		unsigned long cksum = 0;
		for (int i = 0; i < 20; i += 2) {//ͷ��ÿ2�ֽ����
			unsigned short sum = ntohs(*((unsigned short*)(buffer + i)));
			cksum += (unsigned long)sum;
		}
		//printf("1:%x\n",cksum);
		cksum = (cksum >> 16) + (cksum & 0xffff);//����16bit���16bit���
		cksum += cksum >> 16;//ȡ��λ�����16bit���
		unsigned short checksum = (unsigned short)(cksum);//ȡ��16bit
		checksum = ~checksum;
		checksum = htons(checksum);
		memcpy(buffer + 10, &checksum, 2);
		fwd_SendtoLower(buffer, length, it->nexthop);
	}
    return 0;
}
