// SR_server.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdlib.h> 
#include <time.h> 
#include <WinSock2.h> 
#include <fstream> 
#include <io.h>   //C����ͷ�ļ�
#include <iostream>   //for system();

#pragma comment(lib,"ws2_32.lib") 
using namespace std;
#define SERVER_PORT  12340  //�˿ں� 
#define SERVER_IP    "127.0.0.1"  //IP ��ַ 
const int BUFFER_LENGTH = 1026;    //��������С������̫���� UDP ������֡�а�����ӦС�� 1480 �ֽڣ� 
const int SEND_WIND_SIZE = 10;//���ʹ��ڴ�СΪ 10��GBN ��Ӧ����  W + 1 <= N��W Ϊ���ʹ��ڴ�С��N Ϊ���кŸ�����
							  //����ȡ���к� 0...19 �� 20 �� 
							  //��������ڴ�С��Ϊ 1����Ϊͣ-��Э�� 

const int SEQ_SIZE = 20; //���кŵĸ������� 0~19 ���� 20 �� 
						 //���ڷ������ݵ�һ���ֽ����ֵΪ 0�������ݻᷢ��ʧ��
						 //��˽��ն����к�Ϊ 1~20���뷢�Ͷ�һһ��Ӧ 

int ack[SEQ_SIZE];//�յ� ack �������Ӧ 0~19 �� ack 
int curSeq;//��ǰ���ݰ��� seq 
int curAck;//��ǰ�ȴ�ȷ�ϵ� ack 
int totalSeq;//�յ��İ������� 
int totalPacket;//��Ҫ���͵İ����� 

//************************************ 
// Method:        lossInLossRatio 
// FullName:    lossInLossRatio 
// Access:        public   
// Returns:      BOOL 
//  Qualifier:  ���ݶ�ʧ���������һ�����֣��ж��Ƿ�ʧ,��ʧ�򷵻�TRUE�����򷵻� FALSE
// Parameter: float lossRatio [0,1] 
//************************************ 
BOOL lossInLossRatio(float lossRatio) {
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;
	if (r <= lossBound) {
		return TRUE;
	}
	return FALSE;
}

//************************************ 
// Method:        getCurTime 
// FullName:    getCurTime 
// Access:        public   
// Returns:      void 
// Qualifier:  ��ȡ��ǰϵͳʱ�䣬������� ptime �� 
// Parameter: char * ptime 
//************************************ 
void getCurTime(char *ptime) {
	char buffer[128];
	memset(buffer, 0, sizeof(buffer));
	time_t c_time;
	struct tm *p;
	time(&c_time);
	p = localtime(&c_time);
	sprintf_s(buffer, "%d/%d/%d %d:%d:%d",
		p->tm_year + 1900,
		p->tm_mon,
		p->tm_mday,
		p->tm_hour,
		p->tm_min,
		p->tm_sec);
	strcpy_s(ptime, sizeof(buffer), buffer);
}

//************************************ 
// Method:        seqIsAvailable 
// FullName:    seqIsAvailable 
// Access:        public   
// Returns:      bool 
// Qualifier:  ��ǰ���к�  curSeq  �Ƿ���� 
//************************************ 
bool seqIsAvailable() {
	int step;

	step = curSeq - curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;
	//���к��Ƿ��ڵ�ǰ���ʹ���֮�� 
	if (step >= SEND_WIND_SIZE) {
		return false;
	}
	if (ack[curSeq] == 1 || ack[curSeq] == 2) {
		return true;
	}
	return false;
}

//************************************ 
// Method:        timeoutHandler 
// FullName:    timeoutHandler 
// Access:        public   
// Returns:      void 
// Qualifier:  ��ʱ�ش������������������ڵ�����֡��Ҫ�ش� 
//************************************ 
void timeoutHandler(int x) {
	printf("Timer out error.\n");
	//int index, number = 0;
	/*
	for (int i = 0; i< SEND_WIND_SIZE; ++i) {
		index = (i + curAck) % SEQ_SIZE;
		if (ack[index] == 0)
		{
			ack[index] = 2;
			number++;
		}
	}*/
	ack[(x + curAck) % SEQ_SIZE] = 2;
	//totalSeq = totalSeq - number;
	curSeq = curAck;
}

//************************************ 
// Method:        ackHandler 
// FullName:    ackHandler 
// Access:        public   
// Returns:      void 
// Qualifier:  �յ� ack���ۻ�ȷ�ϣ�ȡ����֡�ĵ�һ���ֽ� 
//���ڷ�������ʱ����һ���ֽڣ����кţ�Ϊ 0��ASCII��ʱ����ʧ�ܣ���˼�һ�ˣ��˴���Ҫ��һ��ԭ
// Parameter: char c 
//************************************ 
void ackHandler(char c) {
	unsigned char index = (unsigned char)c - 1; //���кż�һ 
	printf("Recv a ack of %d\n", index);
	if (index == 0) {
		printf("(ackHandler): index = 0\n");
	}
	if (curAck <= index && (curAck + SEND_WIND_SIZE >= SEQ_SIZE || index<curAck + SEND_WIND_SIZE)) {
		ack[index] = 3;
		while (ack[curAck] == 3) {
			ack[curAck] = 1;
			curAck = (curAck + 1) % SEQ_SIZE;
		}
	}
}

//������ 
int main(int argc, char* argv[])
{
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
		printf("WSAStartup failed with error: %d\n", err);
		return -1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else {
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//�����׽���Ϊ������ģʽ 
	int iMode = 1; //1����������0������ 
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) &iMode);//���������� 
	SOCKADDR_IN addrServer;   //��������ַ 
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	//addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//���߾��� 
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	if (err) {
		err = GetLastError();
		printf("Could  not  bind  the  port  %d  for  socket.Error  code is %d\n", SERVER_PORT, err);
		WSACleanup();
		return -1;
	}

	SOCKADDR_IN addrClient;   //�ͻ��˵�ַ 
	int length = sizeof(SOCKADDR);
	char buffer[BUFFER_LENGTH]; //���ݷ��ͽ��ջ����� 
	ZeroMemory(buffer, sizeof(buffer));
	//���������ݶ����ڴ� 
	std::ifstream icin;
	icin.open("../test_server.txt");

	char data[1024 * 113];
	ZeroMemory(data, sizeof(data));
	icin.read(data, 1024 * 113);
	icin.close();
	int handle;
	handle = _open("../test.txt", 0x0100); //open file for read
	long length_lvxiya = _filelength(handle); //get length of file

	totalPacket = length_lvxiya / 1024 + 1;
	int recvSize;
	for (int i = 0; i < SEQ_SIZE; ++i) {
		ack[i] = 1;
	}

	int ret;
	int interval = 1;
	float packetLossRatio = 0;  //Ĭ�ϰ���ʧ�� 0.2 
	float ackLossRatio = 0;  //Ĭ�� ACK ��ʧ�� 0.2 
							 //��ʱ����Ϊ������ӣ�����ѭ���������� 
	srand((unsigned)time(NULL));

	while (true) {
		//���������գ���û���յ����ݣ�����ֵΪ-1 
		recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
		if (recvSize < 0) {
			Sleep(200);
			continue;
		}
		printf("recv from client: %s\n", buffer);
		if (strcmp(buffer, "-time") == 0) {
			getCurTime(buffer);
		}
		else if (strcmp(buffer, "-quit") == 0) {
			strcpy_s(buffer, strlen("Good bye!") + 1, "Good bye!");
		}
		else if (strcmp(buffer, "-testgbn") == 0) {
			//���� gbn ���Խ׶� 
			//���� server��server ���� 0 ״̬���� client ���� 205 ״̬�루server���� 1 ״̬�� 
			//server  �ȴ� client �ظ� 200 ״̬�룬����յ���server ���� 2 ״̬����	��ʼ�����ļ���������ʱ�ȴ�ֱ����ʱ\
																			//���ļ�����׶Σ�server ���ʹ��ڴ�С��Ϊ 
			ZeroMemory(buffer, sizeof(buffer));
			int recvSize;
			//������һ�����ֽ׶� 
			//���ȷ�������ͻ��˷���һ�� 205 ��С��״̬�루���Լ�����ģ���ʾ������׼�����ˣ����Է�������
			//�ͻ����յ� 205 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
			//�������յ� 200 ״̬��֮�󣬾Ϳ�ʼʹ�� GBN ���������� 
			printf("Shake hands stage\n");
			int stage = 0;
			bool runFlag = true;
			printf("%s\n", "Begin to test GBN protocol, please don't abort the process");
			printf("The  loss  ratio  of  packet  is  %.2f,the  loss  ratio  of  ack is %.2f\n", packetLossRatio, ackLossRatio);
			//----------------------------------------------------
			int waitCount[20] = { 0 };//---------------
			BOOL b;
			unsigned short seq = 0;//�������к� 
			unsigned short recvSeq = 0;//��ȷ�ϵ�������к� 
			unsigned short waitSeq = 1;//�ȴ������к� �����ڴ�СΪ10�����Ϊ��С��ֵ
			char buffer_1[SEND_WIND_SIZE][BUFFER_LENGTH];//���յ��Ļ���������
			int i_state = 0;
			for (i_state = 0; i_state < SEND_WIND_SIZE; i_state++)
			{
				ZeroMemory(buffer_1[i_state], sizeof(buffer_1[i_state]));
			}

			BOOL ack_send[SEND_WIND_SIZE];//ack��������ļ�¼����Ӧ1-20��ack,�տ�ʼȫΪfalse
			int success_number = 0;// �����ڳɹ����յĸ���
			for (i_state = 0; i_state < SEND_WIND_SIZE; i_state++) {//��¼��һ���ɹ�������
				ack_send[i_state] = false;
			}
			std::ofstream out_result;
			out_result.open("../result.txt", std::ios::out | std::ios::trunc);
			if (!out_result.is_open()) {
				printf("�ļ���ʧ�ܣ�����\n");
				continue;
			}

			//------------------------------------------------------------------------
			while (runFlag) {
				int recv_lvxy = 0;
				switch (stage) {
				case 0://���� 205 �׶� 
					buffer[0] = 205;
					sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
					Sleep(100);
					stage = 1;
					break;
				case 1://�ȴ����� 200 �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ

					recv_lvxy = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
					if (recv_lvxy < 0) {
						++waitCount[0];
						if (waitCount[0] > 20) {
							runFlag = false;
							printf("Timeout error\n");
							break;
						}
						Sleep(500);
						continue;
					}
					else {
						if ((unsigned char)buffer[0] == 200) {
							printf("Begin a file transfer\n");
							printf("File size is %dB, each packet is 1024B and packet total num is %d\n", sizeof(data), totalPacket);
							curSeq = 0;
							curAck = 0;
							totalSeq = 0;
							for (int x = 0; x < 20; ++x) waitCount[x] = -1;
							stage = 2;
						}
					}
					break;
				case 2://���ݴ���׶� 					
					if (seqIsAvailable()){// && totalSeq - SEND_WIND_SIZE <= totalPacket) {
						b = lossInLossRatio(ackLossRatio);
						//���͸��ͻ��˵����кŴ� 1 ��ʼ 
						buffer[0] = curSeq + 1;
						if (b) {
							printf("The  packet from client  of  %d  loss\n", (unsigned char)buffer[0]);
							break;
						}

						buffer[1] = seq;//-+++++++++++++++++++++�������ǲ���0

						ack[curSeq] = 0;
						waitCount[curSeq] = 0;
						//���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������ 
						//Ϊ�򻯹��̴˴���δʵ�� 
						memcpy(&buffer[2], data + 1024 * (curSeq + (totalSeq / SEND_WIND_SIZE)*SEND_WIND_SIZE), 1024);
						printf("SERVER��send a packet with a seq of %d and a ack of %d\n", curSeq + 1, seq);
						sendto(sockServer, buffer, BUFFER_LENGTH, 0,(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
						++curSeq;
						curSeq %= SEQ_SIZE;
						++totalSeq;
						//Sleep(500);

						recv_lvxy =	recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
						//recv_lvxy =
						//recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
						if (recv_lvxy < 0) {
							for (int x = 0; x < 10; ++x) {
								if (waitCount[x] < 0)continue;
								waitCount[x]++;
								//20 �εȴ� ack ��ʱ�ش� 
								if (waitCount[x] > 20)
								{
									timeoutHandler(x);
									waitCount[x] = 0;
								}
							}
						}
						else {
							b = lossInLossRatio(packetLossRatio);
							if (b) {
								printf("The packet from Client with a seq of %d and ACK for SERVER of %d loss\n", buffer[0], buffer[1]);
								break;
							}
							//ע����ʱack--(unsigned short)buffer[1];�Ļ�Ҫ�����ǲ���0�������0�Ļ���ʾ�Է�û���չ����ݣ��ǲ���ackHandel�ģ�
							if ((unsigned short)buffer[1] != 0) {
								int old_cur_ack = curAck;
								ackHandler(buffer[1]);
								waitCount[(unsigned short)buffer[1]] = -1;
							}
							//--------------------------------������������������������������������
							seq = (unsigned short)buffer[0];
							printf("client packet: %d, ACK for SERVER: %d\n", seq, buffer[1]);
							if (seq >= waitSeq && (waitSeq + SEND_WIND_SIZE > SEQ_SIZE || seq < (waitSeq + SEND_WIND_SIZE))) {//�ڽ��մ��ڷ�Χ��
								memcpy(buffer_1[seq - waitSeq], &buffer[2], sizeof(buffer));
								ack_send[seq - waitSeq] = true;
								int ack_s = 0;
								while (ack_send[ack_s] && ack_s < SEND_WIND_SIZE) {
									//���ϲ㴫������							
									out_result << buffer_1[ack_s];
									printf("%s",buffer_1[ack_s]);
									ZeroMemory(buffer_1[ack_s], sizeof(buffer_1[ack_s]));
									waitSeq++;
									if (waitSeq == 21) {
										waitSeq = 1;
									}
									ack_s = ack_s + 1;
								}
								if (ack_s > 0) {
									for (int i = 0; i < SEND_WIND_SIZE; i++) {
										if (ack_s + i < SEND_WIND_SIZE)
										{
											ack_send[i] = ack_send[i + ack_s];
											memcpy(buffer_1[i], buffer_1[i + ack_s], sizeof(buffer_1[i + ack_s]));
											ZeroMemory(buffer_1[i + ack_s], sizeof(buffer_1[i + ack_s]));
										}
										else
										{
											ack_send[i] = false;
											ZeroMemory(buffer_1[i], sizeof(buffer_1[i]));
										}

									}
								}
								recvSeq = seq;
							}
							buffer[1] = seq;
						}

					}
					else if (curSeq>0 && curSeq - curAck >= 0 ? curSeq - curAck <= SEND_WIND_SIZE : curSeq - curAck + SEQ_SIZE <= SEND_WIND_SIZE && totalSeq - SEND_WIND_SIZE <= totalPacket) {

						curSeq++;
						curSeq %= SEQ_SIZE;
					}
					else {/*
						waitCount++;

						if (waitCount > 20)
						{
							timeoutHandler();
							waitCount = 0;
							//recvSeq = 0;
						}*/
						for (int x = 0; x < 10; ++x) {
							if (waitCount[x] < 0)continue;
							waitCount[x]++;
							//20 �εȴ� ack ��ʱ�ش� 
							if (waitCount[x] > 20)
							{
								timeoutHandler(x);
								waitCount[x] = 0;
							}
						}

					}
					/*
					else if (totalSeq - SEND_WIND_SIZE > totalPacket){
					memcpy(buffer, "good bye\0", 9);
					runFlag = false;
					break;
					}*/
					Sleep(500);
					break;
				}
			}
		success:out_result.close();
		}
		sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient,
			sizeof(SOCKADDR));
		Sleep(500);

	}
	//�ر��׽��֣�ж�ؿ� 
	closesocket(sockServer);
	WSACleanup();
	return 0;
}
