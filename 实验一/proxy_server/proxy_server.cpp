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
#define MAXSIZE 65507 //发送数据报文的最大长度
using namespace std;
//Http 重要头部数据
struct HttpHeader {
	char method[4]; // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
	char url[1024]; // 请求的 url
	char host[1024]; // 目标主机
	char cookie[1024 * 10]; //cookie
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));
	}
};
typedef struct node
{ // 链表，用于缓存资源，资源使用二进制文件保存，与url同名
	char url[1024];
	char last_modified[30];
	char cache[MAXSIZE];
	struct node* next;
} LinkList;

void insert(LinkList* head, LinkList* node)
{ //向链表中插入
	LinkList* t = head->next;
	node->next = t;
	head->next = node;
}

LinkList* search(LinkList* head, char* url)
{ //从链表中寻找
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

//新增函数

int GetPortByName(char* name);
char* InsertTime(char* buf, LinkList* node);
BOOL Modified(char* buf);
void GetTime(char* buf, char* time);
void GetBanned_server(const char* filename);
void GetBanned_client(const char* filename);
void GetFish(const char* filename);

//代理相关参数
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;
//由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
//可以使用线程池技术提高服务器效率
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};
struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
};

//全局变量，用于辅助存储信息
LinkList* cache;
list<string> banned_client;
list<string> banned_server;
list<string> fish;

int _tmain(int argc, _TCHAR* argv[])
{
	printf("代理服务器正在启动\n");
	printf("初始化...\n");
	//开始初始化辅助功能
	//初始化资源缓存链表
	cache = (LinkList*)malloc(sizeof(LinkList));
	cache->next = NULL;
	//初始化用户过滤
	printf("\n开始读取banned_client\n");
	GetBanned_client("banned_client.txt");
	list<string>::iterator iter;
	for (iter = banned_client.begin(); iter != banned_client.end(); iter++) {
		cout << "获取banned_client:" << *iter << endl;
	}
	//初始化网站过滤
	printf("\n开始读取banned_server\n");
	GetBanned_server("banned_server.txt");
	for (iter = banned_server.begin(); iter != banned_server.end(); iter++) {
		cout << "获取banned_server:" << *iter << endl;
	}
	//初始化钓鱼网站
	printf("\n开始读取fish\n");
	GetFish("fish.txt");
	for (iter = fish.begin(); iter != fish.end(); iter++) {
		cout << "fish:" << *iter << endl;
	}
	//辅助功能初始化完毕
	if (!InitSocket()) {
		printf("socket 初始化失败\n");
		return -1;
	}
	printf("\n代理服务器正在运行，监听端口 %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	ProxyParam* lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;
	//代理服务器不断监听
	while (true) {
		sockaddr_in caddr;
		int length = sizeof(caddr);
		acceptSocket = accept(ProxyServer, (sockaddr*)&caddr, &length);
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL) {
			continue;
		}
		string cip = inet_ntoa(caddr.sin_addr);
		//printf("客户%s访问\n", cip);
		BOOL flag = false;
		for (iter = banned_client.begin(); iter != banned_client.end(); iter++) {
			if (cip.compare(*iter) == 0)
			{
				flag = true;
				break;
			}
		}
		if (flag) {
			cout << "禁止客户" << cip << "访问" << endl;
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
// Qualifier: 初始化套接字
//************************************
BOOL InitSocket() {
	//加载套接字库（必须）
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//找不到 winsock.dll
		printf("加载 winsock 失败，错误代码为: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("不能找到正确的 winsock 版本\n");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ProxyServer) {
		printf("创建套接字失败，错误代码为：%d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("绑定套接字失败\n");
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("监听端口%d 失败", ProxyPort);
		return FALSE;
	}
	return TRUE;
}
//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public 
// Returns: unsigned int __stdcall
// Qualifier: 线程执行函数
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
		printf("关闭套接字\n");
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
	//printf("收到报文：\n%s\n", CacheBuffer);
	if (ParseHttpHead(CacheBuffer, httpHeader)) {//Method为其他关闭套接字，防止出现段错误
		printf("Method不为POST或GET，关闭套接字\n");
		Sleep(200);
		closesocket(((ProxyParam*)lpParameter)->clientSocket);
		closesocket(((ProxyParam*)lpParameter)->serverSocket);
		delete lpParameter;
		_endthreadex(0);
		return 0;
	}
	list<string>::iterator iter;
	BOOL flag = false;
	for (iter = banned_server.begin(); iter != banned_server.end(); iter++) {//查找访问的是否是禁止网页
		string str = *iter;
		int len = strcmp((char*)str.c_str(), httpHeader->host);
		if (len >= str.length() || len == 0)
		{
			flag = true;
			break;
		}
	}
	if (flag) {//访问禁止网址，直接退出子线程
		printf("禁止访问，关闭套接字\n");
		Sleep(200);
		closesocket(((ProxyParam*)lpParameter)->clientSocket);
		closesocket(((ProxyParam*)lpParameter)->serverSocket);
		delete lpParameter;
		_endthreadex(0);
		return 0;
	}
	for (iter = fish.begin(); iter != fish.end(); iter ++) {//映射到钓鱼网站
		string str = httpHeader->host;
		if (str.compare(*iter) == 0) {
			if (strcmp(httpHeader->url, "http://hao.360.com/favicon.ico")) {
				char buf[] = "GET http://jwts.hit.edu.cn/ HTTP/1.1\r\nHost: jwts.hit.edu.cn\r\nProxy-Connection: keep-alive\r\nUpgrade-Insecure-Requests : 1\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/95.0.4638.54 Safari/537.36 Edg/95.0.1020.30\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\nPurpose: prefetch\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6\r\n";
				//printf("钓鱼发送:\n%s\n", buf);
				char cache1[1024];
				ZeroMemory(cache1, sizeof(cache1));
				memcpy(cache1, buf, sizeof(buf));
				char host[] = "jwts.hit.edu.cn";
				if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, host)) {
					printf("关闭套接字\n");
					Sleep(200);
					closesocket(((ProxyParam*)lpParameter)->clientSocket);
					closesocket(((ProxyParam*)lpParameter)->serverSocket);
					delete lpParameter;
					_endthreadex(0);
					return 0;
				}
				printf("钓鱼代理连接主机（假） %s 成功\n", httpHeader->host);
				ret = send(((ProxyParam*)lpParameter)->serverSocket, cache1, MAXSIZE, 0);
				recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
				//printf("钓鱼回复：\n%s\n", Buffer);
				ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
			}
			else {
				char buf[] = "GET http://jwts.hit.edu.cn/favicon.ico HTTP/1.1\r\nHost: jwts.hit.edu.cn\r\nProxy-Connection: keep-alive\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/95.0.4638.54 Safari/537.36 Edg/95.0.1020.30\r\nAccept: image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8\r\nReferer: http://jwts.hit.edu.cn/\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6\r\nCookie: name=value; JSESSIONID=6AF17C1B933E2DF90012069BBA42C035; clwz_blc_pst=50335916.20480\r\n";
				//printf("钓鱼发送:\n%s\n", buf);
				char cache1[1024];
				ZeroMemory(cache1, sizeof(cache1));
				memcpy(cache1, buf, sizeof(buf));
				char host[] = "jwts.hit.edu.cn";
				if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, host)) {
					printf("关闭套接字\n");
					Sleep(200);
					closesocket(((ProxyParam*)lpParameter)->clientSocket);
					closesocket(((ProxyParam*)lpParameter)->serverSocket);
					delete lpParameter;
					_endthreadex(0);
					return 0;
				}
				printf("钓鱼代理连接主机（假） %s 成功\n", httpHeader->host);
				ret = send(((ProxyParam*)lpParameter)->serverSocket, cache1, MAXSIZE, 0);
				recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
				//printf("钓鱼回复：\n%s\n", Buffer);
				ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
			}
			printf("钓鱼完成，关闭套接字\n");
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
		printf("关闭套接字\n");
		Sleep(200);
		closesocket(((ProxyParam*)lpParameter)->clientSocket);
		closesocket(((ProxyParam*)lpParameter)->serverSocket);
		delete lpParameter;
		_endthreadex(0);
		return 0;
	}
	printf("代理连接主机 %s 成功\n", httpHeader->host);
	//查看代理服务器是否有资源缓存
	LinkList* node = NULL;
	node = search(cache, httpHeader->url);
	if (node != NULL) {
		//代理服务器有匹配资源
		printf("服务器找到匹配缓存资源:%s\n\n", node->url);
		//添加If-Modified-Since字段
		InsertTime(Buffer, node);
		//将修改后的HTTP 数据报文转发给目标服务器
		ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer, strlen(Buffer)
			+ 1, 0);
		//等待目标服务器返回数据
		recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
		if (recvSize <= 0) {
			printf("关闭套接字\n");
			Sleep(200);
			closesocket(((ProxyParam*)lpParameter)->clientSocket);
			closesocket(((ProxyParam*)lpParameter)->serverSocket);
			delete lpParameter;
			_endthreadex(0);
			return 0;
		}
		if (Modified(Buffer)) {
			//资源被修改
			//将目标服务器返回的数据直接转发给客户端
			ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
			//获得Last-Modified
			char time[] = "Thu, 01 Jul 1970 20:00:00 GMT";
			GetTime(Buffer, time);
			//更新缓存资源和Last-Modified记录
			memcpy(node->last_modified, time, strlen(time)+1);
			memcpy(node->cache, Buffer, sizeof(node->cache));
		}
		else {
			//资源没有被修改
			//将缓存的资源发给客户端
			memcpy(Buffer, node->cache, sizeof(Buffer));
			ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
		}
	}
	else {
		//代理服务器没有缓存
		//将客户端发送的 HTTP 数据报文直接转发给目标服务器
		ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer, strlen(Buffer)
			+ 1, 0);
		//等待目标服务器返回数据
		recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
		if (recvSize <= 0) {
			printf("关闭套接字\n");
			Sleep(200);
			closesocket(((ProxyParam*)lpParameter)->clientSocket);
			closesocket(((ProxyParam*)lpParameter)->serverSocket);
			delete lpParameter;
			_endthreadex(0);
			return 0;
		}
		printf("收到报文服务器：\n%s\n", Buffer);
		//获得Last-Modified
		char time[] = "Thu, 01 Jul 1970 20:00:00 GMT";
		GetTime(Buffer, time);
		//printf("time:%s\n", time);
		//新建资源缓存文件和Last-Modified记录
		printf("创建缓存记录：%s\n\n", httpHeader->url);
		LinkList* node1 = (LinkList*)malloc(sizeof(LinkList));
		memcpy(node1->url, httpHeader->url, strlen(httpHeader->url) + 1);
		memcpy(node1->last_modified, time, strlen(time) + 1);
		memcpy(node1->cache, Buffer, sizeof(node->cache));
		insert(cache, node1);
	}
	printf("关闭套接字\n");
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
// Qualifier: 解析 TCP 报文中的 HTTP 头部
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
BOOL ParseHttpHead(char* buffer, HttpHeader* httpHeader) {
	char* p;
	char* ptr = NULL;
	const char* delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);//提取第一行
	printf("客户请求头：%s\n", p);
	if (p[0] == 'G') {//GET 方式
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
		httpHeader->url[strlen(p) - 13] = '\0';
	}
	else if (p[0] == 'P') {//POST 方式
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
		httpHeader->url[strlen(p) - 14] = '\0';
	}
	else {//Method不为GET或POST直接退出，防止出现错误
		delete httpHeader;
		httpHeader = NULL;
		return true;
	}
	printf("客户请求资源：%s\n\n", httpHeader->url);
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
// Qualifier: 根据主机创建目标服务器套接字，并连接
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
// Qualifier: 从hostname中获得端口号
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
	if (p == NULL)//若没有端口号，则默认为80端口
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
// Qualifier: 向报文中插入If-Modified-Since字段
// Parameter: char* buf
// Parameter: LinkList* node
//************************************
char* InsertTime(char* buf, LinkList* node)
{
	char* p;
	char* ptr = NULL;
	char cpyBuf1[MAXSIZE], cpyBuf2[MAXSIZE]; // 复制报文，strtok会破坏报文结构
	ZeroMemory(cpyBuf1, sizeof(cpyBuf1));
	ZeroMemory(cpyBuf2, sizeof(cpyBuf2));
	memcpy(cpyBuf1, buf, strlen(buf));
	buf = (char*)malloc(strlen(buf) + 51);
	memcpy(buf, cpyBuf1, strlen(cpyBuf1));

	const char* delim = "\r\n";
	p = strtok_s(buf, delim, &ptr); //提取第一行

	int len = strlen(p);
	int len1 = len + strlen("\r\nIf-Modified-Since: ") + 1;
	strcat_s(p, len1, "\r\nIf-Modified-Since: ");//添加If-Modified-Since字段
	len1 = strlen(p) + strlen(node->last_modified) + 1;
	strcat_s(p, len1, node->last_modified);//添加最后修改时间
	//printf("node->Last-Modified:%s\n", node->last_modified);
	len1 = strlen(p) + 3;
	strcat_s(p, len1, "\r\n");
	memcpy(cpyBuf2, &cpyBuf1[len + 2], strlen(cpyBuf1) - len - 2);
	len1 = strlen(p) + strlen(cpyBuf2) + 1;
	strcat_s(p, len1, cpyBuf2);
	printf("插入If-Modified-Since后：\n%s\n\n", buf);
	return buf;
}

//************************************
// Method: Modified
// FullName: Modified
// Access: public 
// Returns: BOOL
// Qualifier: 根据服务器响应报文状态码检查资源是否修改
// Parameter: char* buf
//************************************
BOOL Modified(char* buf)
{
	char* ptr = NULL;
	char* p, q[4];
	char buffer[MAXSIZE]; // 复制报文，strtok会破坏报文结构
	ZeroMemory(buffer, sizeof(buffer));
	ZeroMemory(q, 4);
	memcpy(buffer, buf, strlen(buf));

	const char* delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr); //提取第一行

	delim = "\0";
	// q = strtok(p, delim);
	// printf("q第一段:%s\n",q);
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
// Qualifier: 从服务器响应报文中获得Last-Modified字段
// Parameter: char* buf
// Parameter: char* time
//************************************
void GetTime(char* buf, char* time)
{
	char* ptr = NULL;
	char* p;
	char buffer[MAXSIZE]; // 复制报文，strtok会破坏报文结构
	ZeroMemory(buffer, sizeof(buffer));
	memcpy(buffer, buf, strlen(buf));

	const char* delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr); //提取第一行
	p = strtok_s(NULL, delim, &ptr);   //提取剩余响应头部
	while (p)                  //循环识别相应头部每一行的字段
	{

		if (p[0] == 'L'&&p[1]=='a')
		{ //寻找“Last-Modified”
			memcpy(time, &p[15], 30);
			printf("提取Last-Modified:%s\n\n", time);
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
// Qualifier: 从文件中获得过滤网站，文件中每行一个网站
// Parameter: const char* filename
//************************************
void GetBanned_server(const char* filename) {
	ifstream in(filename);
	string line;

	if (in) // 有该文件  
	{
		while (getline(in, line)) // line中不包括每行的换行符  
		{
			banned_server.push_front(line);
		}
	}
	else // 没有该文件  
	{
		cout << "no such file" << endl;
	}
}

//************************************
// Method: GetBanned_client
// FullName: GetBanned_client
// Access: public 
// Returns: void
// Qualifier: 从文件中获得过滤用户，文件中每行一个用户IP
// Parameter: const char* filename
//************************************
void GetBanned_client(const char* filename)
{
	ifstream in(filename);
	string line;

	if (in) // 有该文件  
	{
		while (getline(in, line)) // line中不包括每行的换行符  
		{
			banned_client.push_front(line);
		}
	}
	else // 没有该文件  
	{
		cout << "no such file" << endl;
	}
}

//************************************
// Method: GetFish
// FullName: GetFish
// Access: public 
// Returns: void
// Qualifier: 从文件中获得从网站到钓鱼网站，文件中每行一个被钓鱼的网站
// Parameter: const char* filename
//************************************
void GetFish(const char* filename) {
	ifstream in(filename);
	string line;
	if (in) // 有该文件  
	{
		while (getline(in, line)) // line中不包括每行的换行符  
		{
			fish.push_front(line);
		}
	}
	else // 没有该文件  
	{
		cout << "no such file" << endl;
	}
}