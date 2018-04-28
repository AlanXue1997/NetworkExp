//#include "stdafx.h"
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include "tchar.h"
#pragma comment(lib,"Ws2_32.lib")
#define MAXSIZE 65507 //发送数据报文的最大长度
#define HTTP_PORT 80 //http 服务器端口

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

#define cache_size 1000

struct CACHE_ITEM {
	char url[1024];
	char buffer[65507];
	char last_modified_date[30];
};

CACHE_ITEM cache[cache_size];
int cache_flag = 0;

struct ADV_P {
	struct ProxyParam* lpProxyParam;
	char* IP_addr;
};

BOOL InitSocket();
void ParseHttpHead(char *buffer, HttpHeader * httpHeader, char *IP_addr);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID adv_p);
//代理相关参数
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;

//IP白名单
char* WhiteIP[] = {
	"172.20.12.134",
	NULL
};

//IP黑名单
char* BlackIP[] = {
	"172.20.27.65",
	NULL
};

//限制IP，只有白名单中的地址才能访问
char* LimitedAddr[] = {
	"http://jwes.hit.edu.cn/",
	NULL
};

//禁止地址，所有客户均不能访问
char* ForbiddenAddr[] = {
	"http://today.hit.edu.cn/",
	NULL
};

int phishing = 1;
char Phishing_addr[] = "sougou.com";
char Phishing_addr_http[] = "http://www.sougou.com/ HTTP/1.1";

//由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
//可以使用线程池技术提高服务器效率
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};
struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
};

int _tmain(int argc, _TCHAR* argv[])
{
	printf("代理服务器正在启动\n");
	printf("初始化...\n");
	if (!InitSocket()) {
		printf("socket 初始化失败\n");
		return -1;
	}
	printf("代理服务器正在运行，监听端口 %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	ProxyParam *lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;
	//代理服务器不断监听
	while (true) {
		sockaddr_in addr;
		int len = sizeof(addr);
		acceptSocket = accept(ProxyServer, (sockaddr*)&addr, &len);
		char *ip_addr = new char[16];
		strcpy(ip_addr, inet_ntoa(addr.sin_addr));
		printf("--->>> %s\n", inet_ntoa(addr.sin_addr));
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL) {
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)(new ADV_P{ lpProxyParam, ip_addr}), 0, 0);
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
unsigned int __stdcall ProxyThread(LPVOID adv_p) {
	char * IP_addr = ((ADV_P*)adv_p)->IP_addr;
	ProxyParam *lpParameter = ((ADV_P*)adv_p)->lpProxyParam;
	char Buffer[MAXSIZE];
	char *CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0) {
		goto error;
	}
	HttpHeader* httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	ParseHttpHead(CacheBuffer, httpHeader, IP_addr);
	delete CacheBuffer;
	if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, httpHeader->host)) {
		goto error;
	}
	printf("代理连接主机 %s 成功\n", httpHeader->host);
	
	int found = 0;
	if (strcmp(httpHeader->url, "http://today.hit.edu.cn/") == 0) {
		int ccc;
		ccc = 0;
	}
	for (int i = 0; i < cache_size; ++i) {
		if (cache[i].url != NULL && strcmp(httpHeader->url, cache[i].url) == 0) {
			found = 1;
			int not_modified = 1;
			int length = strlen(Buffer);
			char * pr = Buffer + length;
			memcpy(pr, "If-modified-since: ", 19);
			pr += 19;
			length = strlen(cache[i].last_modified_date);
			memcpy(pr, cache[i].last_modified_date, length);
			ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);

			recvSize = recv(((ProxyParam *)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
			if (recvSize <= 0) {
				goto error;
			}

			const char *blank = " ";
			const char *Modd = "304";
			if (!memcmp(&Buffer[9], Modd, strlen(Modd)))
			{
				ret = send(((ProxyParam*)lpParameter)->clientSocket, cache[i].buffer, strlen(cache[i].buffer) + 1, 0);
				break;
			}
			char *cacheBuff = new char[MAXSIZE];
			ZeroMemory(cacheBuff, MAXSIZE);
			memcpy(cacheBuff, Buffer, MAXSIZE);
			const char *delim = "\r\n";
			char *ptr;
			//char dada[DATELENGTH];
			//ZeroMemory(dada, sizeof(dada));
			char *p = strtok_s(cacheBuff, delim, &ptr);
			while (p) {
				if (p[0] == 'L') {
					if (strlen(p) > 15) {
						if (!(strcmp(cache[i].last_modified_date, "Last-Modified:")))
						{
							memcpy(cache[i].last_modified_date, &p[15], strlen(p) - 15);
							not_modified = 0;
							break;
						}
					}
				}
				p = strtok_s(NULL, delim, &ptr);
			}
			memcpy(cache[i].url, httpHeader->url, sizeof(httpHeader->url));
			memcpy(cache[i].buffer, Buffer, sizeof(Buffer));
			ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);

			break;
		}
	}
	if (!found) {
		//将客户端发送的 HTTP 数据报文直接转发给目标服务器
		ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
		//等待目标服务器返回数据
		recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
		if (recvSize <= 0) {
			goto error;
		}
		//将目标服务器返回的数据直接转发给客户端
		ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
		//if (strcmp(httpHeader->url, "http://today.hit.edu.cn/") == 0) {
			cache_flag = (cache_flag + 1) % cache_size;
			memcpy(cache[cache_flag].url, httpHeader->url, sizeof(httpHeader->url));
			memcpy(cache[cache_flag].buffer, Buffer, sizeof(Buffer));

			char *cacheBuff = new char[MAXSIZE];
			ZeroMemory(cacheBuff, MAXSIZE);
			memcpy(cacheBuff, Buffer, MAXSIZE);
			const char *delim = "\r\n";
			char *ptr;
			//char dada[DATELENGTH];
			//ZeroMemory(dada, sizeof(dada));
			char *p = strtok_s(cacheBuff, delim, &ptr);
			while (p) {
				if (p[0] == 'L') {
					if (strlen(p) > 15) {
						memcpy(cache[cache_flag].last_modified_date, &p[15], strlen(p) - 15);
						break;
					}
				}
				p = strtok_s(NULL, delim, &ptr);
			}
		//}	
	}
	//错误处理
error:
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
void ParseHttpHead(char *buffer, HttpHeader * httpHeader, char *IP_addr) {
	char *p;
	char *ptr;
	const char * delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);//提取第一行
	printf("%s\n", p);
	printf("%s\n", httpHeader->url);
	int flag = 0;
	int black = 0;
	while (BlackIP[flag] != NULL) {
		if (strcmp(BlackIP[flag], IP_addr) == 0) {
			black = 1;
			if (phishing) break;
			else return;
		}
		flag++;
	}
	if (p[0] == 'G') {//GET 方式
		memcpy(httpHeader->method, "GET", 3);
		//p = "GET http://www.sogou.com/ HTTP/1.1";
		if (black) memcpy(httpHeader->url, &Phishing_addr_http, sizeof(Phishing_addr_http));
		else memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P') {//POST 方式
		memcpy(httpHeader->method, "POST", 4);
		if (black) memcpy(httpHeader->url, &Phishing_addr_http, sizeof(Phishing_addr_http));
		else memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	if (!black) {
		flag = 0;
		while (ForbiddenAddr[flag] != NULL) {
			if (strcmp(ForbiddenAddr[flag], httpHeader->url) == 0) {
				return;
			}
			flag++;
		}
		flag = 0;
		int limited = 0;
		while (LimitedAddr[flag] != NULL) {
			if (strcmp(LimitedAddr[flag], httpHeader->url) == 0) {
				limited = 1;
				break;
			}
			flag++;
		}
		if (limited) {
			flag = 0;
			while (WhiteIP[flag] != NULL) {
				if (strcmp(WhiteIP[flag], IP_addr) == 0) {
					limited = 0;
				}
				flag++;
			}
		}
		if (limited) return;
	}
	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		switch (p[0]) {
		case 'H'://Host
			printf("--->>> host : %s", p);
			//p="Host: www.sogou.com";
			if (black)memcpy(httpHeader->host, &Phishing_addr, sizeof(Phishing_addr));
			else memcpy(httpHeader->host, &p[6], strlen(p) - 6);
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
BOOL ConnectToServer(SOCKET *serverSocket, char *host) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	HOSTENT *hostent = gethostbyname(host);
	if (!hostent) {
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET) {
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr))
		== SOCKET_ERROR) {
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}