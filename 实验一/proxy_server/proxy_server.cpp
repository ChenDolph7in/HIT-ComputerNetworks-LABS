#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <string>
//#include <map>
#include <list>
#pragma comment(lib,"Ws2_32.lib")
#define MAXSIZE 65507 //�������ݱ��ĵ���󳤶�
using namespace std;
//Http ��Ҫͷ������
struct HttpHeader {
	char method[4]; // POST ���� GET��ע����ЩΪ CONNECT����ʵ���ݲ�����
	char url[1024]; // ����� url
	char host[1024]; // Ŀ������
	char cookie[1024 * 10]; //cookie
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));
	}
};
typedef struct node
{ // �������ڻ�����Դ����Դʹ�ö������ļ����棬��urlͬ��
	char url[1024];
	char last_modified[30];
	char cache[MAXSIZE];
	struct node* next;
} LinkList;

void insert(LinkList* head, LinkList* node)
{ //�������в���
	LinkList* t = head->next;
	node->next = t;
	head->next = node;
}

LinkList* search(LinkList* head, char* url)
{ //��������Ѱ��
	LinkList* t = head->next;
	while (t != NULL)
	{
		if (strcmp(t->url, url) == 0)
		{
			break;
		}
		t = t->next;
	}
	if (t != NULL)
	{
		return t;
	}
	else
	{
		return NULL;
	}
}

BOOL InitSocket();
BOOL ParseHttpHead(char* buffer, HttpHeader* httpHeader);
BOOL ConnectToServer(SOCKET* serverSocket, char* host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);

//��������

int GetPortByName(char* name);
char* InsertTime(char* buf, LinkList* node);
BOOL Modified(char* buf);
void GetTime(char* buf, char* time);
void GetBanned_server(const char* filename);
void GetBanned_client(const char* filename);
void GetFish(const char* filename);

//������ز���
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;
//�����µ����Ӷ�ʹ�����߳̽��д������̵߳�Ƶ���Ĵ����������ر��˷���Դ
//����ʹ���̳߳ؼ�����߷�����Ч��
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};
struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
};

//ȫ�ֱ��������ڸ����洢��Ϣ
LinkList* cache;
list<string> banned_client;
list<string> banned_server;
list<string> fish;

int _tmain(int argc, _TCHAR* argv[])
{
	printf("�����������������\n");
	printf("��ʼ��...\n");
	//��ʼ��ʼ����������
	//��ʼ����Դ��������
	cache = (LinkList*)malloc(sizeof(LinkList));
	cache->next = NULL;
	//��ʼ���û�����
	printf("\n��ʼ��ȡbanned_client\n");
	GetBanned_client("banned_client.txt");
	list<string>::iterator iter;
	for (iter = banned_client.begin(); iter != banned_client.end(); iter++) {
		cout << "��ȡbanned_client:" << *iter << endl;
	}
	//��ʼ����վ����
	printf("\n��ʼ��ȡbanned_server\n");
	GetBanned_server("banned_server.txt");
	for (iter = banned_server.begin(); iter != banned_server.end(); iter++) {
		cout << "��ȡbanned_server:" << *iter << endl;
	}
	//��ʼ��������վ
	printf("\n��ʼ��ȡfish\n");
	GetFish("fish.txt");
	for (iter = fish.begin(); iter != fish.end(); iter++) {
		cout << "fish:" << *iter << endl;
	}
	//�������ܳ�ʼ�����
	if (!InitSocket()) {
		printf("socket ��ʼ��ʧ��\n");
		return -1;
	}
	printf("\n����������������У������˿� %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	ProxyParam* lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;
	//������������ϼ���
	while (true) {
		sockaddr_in caddr;
		int length = sizeof(caddr);
		acceptSocket = accept(ProxyServer, (sockaddr*)&caddr, &length);
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL) {
			continue;
		}
		string cip = inet_ntoa(caddr.sin_addr);
		//printf("�ͻ�%s����\n", cip);
		BOOL flag = false;
		for (iter = banned_client.begin(); iter != banned_client.end(); iter++) {
			if (cip.compare(*iter) == 0)
			{
				flag = true;
				break;
			}
		}
		if (flag) {
			cout << "��ֹ�ͻ�" << cip << "����" << endl;
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		Sleep(200);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}
//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public 
// Returns: BOOL
// Qualifier: ��ʼ���׽���
//************************************
BOOL InitSocket() {
	//�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ
	int err;
	//�汾 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//�Ҳ��� winsock.dll
		printf("���� winsock ʧ�ܣ��������Ϊ: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("�����ҵ���ȷ�� winsock �汾\n");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ProxyServer) {
		printf("�����׽���ʧ�ܣ��������Ϊ��%d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("���׽���ʧ��\n");
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("�����˿�%d ʧ��", ProxyPort);
		return FALSE;
	}
	return TRUE;
}
//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public 
// Returns: unsigned int __stdcall
// Qualifier: �߳�ִ�к���
// Parameter: LPVOID lpParameter
//************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
	char Buffer[MAXSIZE];
	char* CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	HttpHeader* httpHeader = new HttpHeader();
	recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0) {
		printf("�ر��׽���\n");
		Sleep(200);
		closesocket(((ProxyParam*)lpParameter)->clientSocket);
		closesocket(((ProxyParam*)lpParameter)->serverSocket);
		delete lpParameter;
		_endthreadex(0);
		return 0;
	}
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	//printf("�յ����ģ�\n%s\n", CacheBuffer);
	if (ParseHttpHead(CacheBuffer, httpHeader)) {//MethodΪ�����ر��׽��֣���ֹ���ֶδ���
		printf("Method��ΪPOST��GET���ر��׽���\n");
		Sleep(200);
		closesocket(((ProxyParam*)lpParameter)->clientSocket);
		closesocket(((ProxyParam*)lpParameter)->serverSocket);
		delete lpParameter;
		_endthreadex(0);
		return 0;
	}
	list<string>::iterator iter;
	BOOL flag = false;
	for (iter = banned_server.begin(); iter != banned_server.end(); iter++) {//���ҷ��ʵ��Ƿ��ǽ�ֹ��ҳ
		string str = *iter;
		int len = strcmp((char*)str.c_str(), httpHeader->host);
		if (len >= str.length() || len == 0)
		{
			flag = true;
			break;
		}
	}
	if (flag) {//���ʽ�ֹ��ַ��ֱ���˳����߳�
		printf("��ֹ���ʣ��ر��׽���\n");
		Sleep(200);
		closesocket(((ProxyParam*)lpParameter)->clientSocket);
		closesocket(((ProxyParam*)lpParameter)->serverSocket);
		delete lpParameter;
		_endthreadex(0);
		return 0;
	}
	for (iter = fish.begin(); iter != fish.end(); iter ++) {//ӳ�䵽������վ
		string str = httpHeader->host;
		if (str.compare(*iter) == 0) {
			if (strcmp(httpHeader->url, "http://hao.360.com/favicon.ico")) {
				char buf[] = "GET http://jwts.hit.edu.cn/ HTTP/1.1\r\nHost: jwts.hit.edu.cn\r\nProxy-Connection: keep-alive\r\nUpgrade-Insecure-Requests : 1\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/95.0.4638.54 Safari/537.36 Edg/95.0.1020.30\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\nPurpose: prefetch\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6\r\n";
				//printf("���㷢��:\n%s\n", buf);
				char cache1[1024];
				ZeroMemory(cache1, sizeof(cache1));
				memcpy(cache1, buf, sizeof(buf));
				char host[] = "jwts.hit.edu.cn";
				if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, host)) {
					printf("�ر��׽���\n");
					Sleep(200);
					closesocket(((ProxyParam*)lpParameter)->clientSocket);
					closesocket(((ProxyParam*)lpParameter)->serverSocket);
					delete lpParameter;
					_endthreadex(0);
					return 0;
				}
				printf("������������������٣� %s �ɹ�\n", httpHeader->host);
				ret = send(((ProxyParam*)lpParameter)->serverSocket, cache1, MAXSIZE, 0);
				recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
				//printf("����ظ���\n%s\n", Buffer);
				ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
			}
			else {
				char buf[] = "GET http://jwts.hit.edu.cn/favicon.ico HTTP/1.1\r\nHost: jwts.hit.edu.cn\r\nProxy-Connection: keep-alive\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/95.0.4638.54 Safari/537.36 Edg/95.0.1020.30\r\nAccept: image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8\r\nReferer: http://jwts.hit.edu.cn/\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6\r\nCookie: name=value; JSESSIONID=6AF17C1B933E2DF90012069BBA42C035; clwz_blc_pst=50335916.20480\r\n";
				//printf("���㷢��:\n%s\n", buf);
				char cache1[1024];
				ZeroMemory(cache1, sizeof(cache1));
				memcpy(cache1, buf, sizeof(buf));
				char host[] = "jwts.hit.edu.cn";
				if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, host)) {
					printf("�ر��׽���\n");
					Sleep(200);
					closesocket(((ProxyParam*)lpParameter)->clientSocket);
					closesocket(((ProxyParam*)lpParameter)->serverSocket);
					delete lpParameter;
					_endthreadex(0);
					return 0;
				}
				printf("������������������٣� %s �ɹ�\n", httpHeader->host);
				ret = send(((ProxyParam*)lpParameter)->serverSocket, cache1, MAXSIZE, 0);
				recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
				//printf("����ظ���\n%s\n", Buffer);
				ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
			}
			printf("������ɣ��ر��׽���\n");
			Sleep(200);
			closesocket(((ProxyParam*)lpParameter)->clientSocket);
			closesocket(((ProxyParam*)lpParameter)->serverSocket);
			delete lpParameter;
			_endthreadex(0);
			return 0;
		}
	}
	delete CacheBuffer;
	if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, httpHeader->host)) {
		printf("�ر��׽���\n");
		Sleep(200);
		closesocket(((ProxyParam*)lpParameter)->clientSocket);
		closesocket(((ProxyParam*)lpParameter)->serverSocket);
		delete lpParameter;
		_endthreadex(0);
		return 0;
	}
	printf("������������ %s �ɹ�\n", httpHeader->host);
	//�鿴����������Ƿ�����Դ����
	LinkList* node = NULL;
	node = search(cache, httpHeader->url);
	if (node != NULL) {
		//�����������ƥ����Դ
		printf("�������ҵ�ƥ�仺����Դ:%s\n\n", node->url);
		//���If-Modified-Since�ֶ�
		InsertTime(Buffer, node);
		//���޸ĺ��HTTP ���ݱ���ת����Ŀ�������
		ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer, strlen(Buffer)
			+ 1, 0);
		//�ȴ�Ŀ���������������
		recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
		if (recvSize <= 0) {
			printf("�ر��׽���\n");
			Sleep(200);
			closesocket(((ProxyParam*)lpParameter)->clientSocket);
			closesocket(((ProxyParam*)lpParameter)->serverSocket);
			delete lpParameter;
			_endthreadex(0);
			return 0;
		}
		if (Modified(Buffer)) {
			//��Դ���޸�
			//��Ŀ����������ص�����ֱ��ת�����ͻ���
			ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
			//���Last-Modified
			char time[] = "Thu, 01 Jul 1970 20:00:00 GMT";
			GetTime(Buffer, time);
			//���»�����Դ��Last-Modified��¼
			memcpy(node->last_modified, time, strlen(time)+1);
			memcpy(node->cache, Buffer, sizeof(node->cache));
		}
		else {
			//��Դû�б��޸�
			//���������Դ�����ͻ���
			memcpy(Buffer, node->cache, sizeof(Buffer));
			ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
		}
	}
	else {
		//���������û�л���
		//���ͻ��˷��͵� HTTP ���ݱ���ֱ��ת����Ŀ�������
		ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer, strlen(Buffer)
			+ 1, 0);
		//�ȴ�Ŀ���������������
		recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
		if (recvSize <= 0) {
			printf("�ر��׽���\n");
			Sleep(200);
			closesocket(((ProxyParam*)lpParameter)->clientSocket);
			closesocket(((ProxyParam*)lpParameter)->serverSocket);
			delete lpParameter;
			_endthreadex(0);
			return 0;
		}
		printf("�յ����ķ�������\n%s\n", Buffer);
		//���Last-Modified
		char time[] = "Thu, 01 Jul 1970 20:00:00 GMT";
		GetTime(Buffer, time);
		//printf("time:%s\n", time);
		//�½���Դ�����ļ���Last-Modified��¼
		printf("���������¼��%s\n\n", httpHeader->url);
		LinkList* node1 = (LinkList*)malloc(sizeof(LinkList));
		memcpy(node1->url, httpHeader->url, strlen(httpHeader->url) + 1);
		memcpy(node1->last_modified, time, strlen(time) + 1);
		memcpy(node1->cache, Buffer, sizeof(node->cache));
		insert(cache, node1);
	}
	printf("�ر��׽���\n");
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	_endthreadex(0);
	return 0;
}
//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public 
// Returns: void
// Qualifier: ���� TCP �����е� HTTP ͷ��
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
BOOL ParseHttpHead(char* buffer, HttpHeader* httpHeader) {
	char* p;
	char* ptr = NULL;
	const char* delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);//��ȡ��һ��
	printf("�ͻ�����ͷ��%s\n", p);
	if (p[0] == 'G') {//GET ��ʽ
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
		httpHeader->url[strlen(p) - 13] = '\0';
	}
	else if (p[0] == 'P') {//POST ��ʽ
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
		httpHeader->url[strlen(p) - 14] = '\0';
	}
	else {//Method��ΪGET��POSTֱ���˳�����ֹ���ִ���
		delete httpHeader;
		httpHeader = NULL;
		return true;
	}
	printf("�ͻ�������Դ��%s\n\n", httpHeader->url);
	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		switch (p[0]) {
		case 'H'://Host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			break;
		case 'C'://Cookie
			if (strlen(p) > 8) {
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")) {
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
	return false;
}
//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public 
// Returns: BOOL
// Qualifier: ������������Ŀ��������׽��֣�������
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET* serverSocket, char* host) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(GetPortByName(host));
	HOSTENT* hostent = gethostbyname(host);
	if (!hostent) {
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET) {
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr))
		== SOCKET_ERROR) {
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}

//************************************
// Method: GetPortByName
// FullName: GetPortByName
// Access: public 
// Returns: int
// Qualifier: ��hostname�л�ö˿ں�
// Parameter: char* name
//************************************
int GetPortByName(char* name)
{
	char* ptr = NULL;
	char* cpy = (char*)malloc(strlen(name));
	memcpy(cpy, name, strlen(name));
	const char* delim = ":";
	strtok_s(cpy, delim, &ptr);
	char* p = strtok_s(NULL, delim, &ptr);
	if (p == NULL)//��û�ж˿ںţ���Ĭ��Ϊ80�˿�
	{
		return 80;
	}
	else
	{
		return atoi(p);
	}
}

//************************************
// Method: InsertTime
// FullName: InsertTime
// Access: public 
// Returns: char*
// Qualifier: �����в���If-Modified-Since�ֶ�
// Parameter: char* buf
// Parameter: LinkList* node
//************************************
char* InsertTime(char* buf, LinkList* node)
{
	char* p;
	char* ptr = NULL;
	char cpyBuf1[MAXSIZE], cpyBuf2[MAXSIZE]; // ���Ʊ��ģ�strtok���ƻ����Ľṹ
	ZeroMemory(cpyBuf1, sizeof(cpyBuf1));
	ZeroMemory(cpyBuf2, sizeof(cpyBuf2));
	memcpy(cpyBuf1, buf, strlen(buf));
	buf = (char*)malloc(strlen(buf) + 51);
	memcpy(buf, cpyBuf1, strlen(cpyBuf1));

	const char* delim = "\r\n";
	p = strtok_s(buf, delim, &ptr); //��ȡ��һ��

	int len = strlen(p);
	int len1 = len + strlen("\r\nIf-Modified-Since: ") + 1;
	strcat_s(p, len1, "\r\nIf-Modified-Since: ");//���If-Modified-Since�ֶ�
	len1 = strlen(p) + strlen(node->last_modified) + 1;
	strcat_s(p, len1, node->last_modified);//�������޸�ʱ��
	//printf("node->Last-Modified:%s\n", node->last_modified);
	len1 = strlen(p) + 3;
	strcat_s(p, len1, "\r\n");
	memcpy(cpyBuf2, &cpyBuf1[len + 2], strlen(cpyBuf1) - len - 2);
	len1 = strlen(p) + strlen(cpyBuf2) + 1;
	strcat_s(p, len1, cpyBuf2);
	printf("����If-Modified-Since��\n%s\n\n", buf);
	return buf;
}

//************************************
// Method: Modified
// FullName: Modified
// Access: public 
// Returns: BOOL
// Qualifier: ���ݷ�������Ӧ����״̬������Դ�Ƿ��޸�
// Parameter: char* buf
//************************************
BOOL Modified(char* buf)
{
	char* ptr = NULL;
	char* p, q[4];
	char buffer[MAXSIZE]; // ���Ʊ��ģ�strtok���ƻ����Ľṹ
	ZeroMemory(buffer, sizeof(buffer));
	ZeroMemory(q, 4);
	memcpy(buffer, buf, strlen(buf));

	const char* delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr); //��ȡ��һ��

	delim = "\0";
	// q = strtok(p, delim);
	// printf("q��һ��:%s\n",q);
	// q = strtok(NULL, delim);
	memcpy(q, &p[9], 3);

	if (strncmp(q, "200", 3))
	{
		return true;
	}
	else
	{
		return false;
	}
}

//************************************
// Method: GetTime
// FullName: GetTime
// Access: public 
// Returns: void
// Qualifier: �ӷ�������Ӧ�����л��Last-Modified�ֶ�
// Parameter: char* buf
// Parameter: char* time
//************************************
void GetTime(char* buf, char* time)
{
	char* ptr = NULL;
	char* p;
	char buffer[MAXSIZE]; // ���Ʊ��ģ�strtok���ƻ����Ľṹ
	ZeroMemory(buffer, sizeof(buffer));
	memcpy(buffer, buf, strlen(buf));

	const char* delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr); //��ȡ��һ��
	p = strtok_s(NULL, delim, &ptr);   //��ȡʣ����Ӧͷ��
	while (p)                  //ѭ��ʶ����Ӧͷ��ÿһ�е��ֶ�
	{

		if (p[0] == 'L'&&p[1]=='a')
		{ //Ѱ�ҡ�Last-Modified��
			memcpy(time, &p[15], 30);
			printf("��ȡLast-Modified:%s\n\n", time);
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
}

//************************************
// Method: GetBanned_server
// FullName: GetBanned_server
// Access: public 
// Returns: void
// Qualifier: ���ļ��л�ù�����վ���ļ���ÿ��һ����վ
// Parameter: const char* filename
//************************************
void GetBanned_server(const char* filename) {
	ifstream in(filename);
	string line;

	if (in) // �и��ļ�  
	{
		while (getline(in, line)) // line�в�����ÿ�еĻ��з�  
		{
			banned_server.push_front(line);
		}
	}
	else // û�и��ļ�  
	{
		cout << "no such file" << endl;
	}
}

//************************************
// Method: GetBanned_client
// FullName: GetBanned_client
// Access: public 
// Returns: void
// Qualifier: ���ļ��л�ù����û����ļ���ÿ��һ���û�IP
// Parameter: const char* filename
//************************************
void GetBanned_client(const char* filename)
{
	ifstream in(filename);
	string line;

	if (in) // �и��ļ�  
	{
		while (getline(in, line)) // line�в�����ÿ�еĻ��з�  
		{
			banned_client.push_front(line);
		}
	}
	else // û�и��ļ�  
	{
		cout << "no such file" << endl;
	}
}

//************************************
// Method: GetFish
// FullName: GetFish
// Access: public 
// Returns: void
// Qualifier: ���ļ��л�ô���վ��������վ���ļ���ÿ��һ�����������վ
// Parameter: const char* filename
//************************************
void GetFish(const char* filename) {
	ifstream in(filename);
	string line;
	if (in) // �и��ļ�  
	{
		while (getline(in, line)) // line�в�����ÿ�еĻ��з�  
		{
			fish.push_front(line);
		}
	}
	else // û�и��ļ�  
	{
		cout << "no such file" << endl;
	}
}