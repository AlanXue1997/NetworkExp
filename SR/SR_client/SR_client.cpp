// SR_client.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h> 
#include <WinSock2.h> 
#include <time.h> 
#include <fstream> 
#include <io.h>   //C����ͷ�ļ�
#include <iostream>   //for system();
using namespace std;
#pragma comment(lib,"ws2_32.lib") 

#define SERVER_PORT  12340 //�������ݵĶ˿ں� 
#define SERVER_PORT_recv  10240
#define SERVER_IP    "127.0.0.1" //  �������� IP ��ַ 

const int BUFFER_LENGTH = 1026;
const int SEQ_SIZE = 20;//���ն����кŸ�����Ϊ 1~20 
const int RECV_WIND_SIZE = 10;//���մ��ڵĴ�С����ҪС�ڵ�����Ŵ�С��һ��
							  //���ڷ������ݵ�һ���ֽ����ֵΪ 0�������ݻᷢ��ʧ��
							  //��˽��ն����к�Ϊ 1~20���뷢�Ͷ�һһ��Ӧ 

int ack[SEQ_SIZE];//�յ� ack �������Ӧ 0~19 �� ack 
int curSeq;//��ǰ���ݰ��� seq 
int curAck;//��ǰ�ȴ�ȷ�ϵ� ack 
int totalSeq;//�յ��İ������� 
int totalPacket;//��Ҫ���͵İ����� 

/****************************************************************/
/*            -time  �ӷ������˻�ȡ��ǰʱ��
-quit  �˳��ͻ���
-testgbn [X]  ���� GBN Э��ʵ�ֿɿ����ݴ���
[X] [0,1]  ģ�����ݰ���ʧ�ĸ���
[Y] [0,1]  ģ�� ACK ��ʧ�ĸ���
*/
/****************************************************************/
void printTips() {
	printf("*****************************************\n");
	printf("|-time to get current time              |\n");
	printf("|-quit to exit client                   |\n");
	printf("|-testgbn [X] [Y] to test the gbn       |\n");
	printf("*****************************************\n");
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
	if (step >= RECV_WIND_SIZE) {
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
void timeoutHandler() {
	printf("Timer out error.\n");
	int index, number = 0;
	for (int i = 0; i< RECV_WIND_SIZE; ++i) {
		index = (i + curAck) % SEQ_SIZE;
		if (ack[index] == 0)
		{
			ack[index] = 2;
			number++;
		}
	}
	totalSeq = totalSeq - number;
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
	if (curAck <= index && (curAck + RECV_WIND_SIZE >= SEQ_SIZE ? true : index<curAck + RECV_WIND_SIZE)) {

		ack[index] = 3;
		while (ack[curAck] == 3) {
			ack[curAck] = 1;
			curAck = (curAck + 1) % SEQ_SIZE;

		}
	}
}
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
		return 1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else {
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	//�����׽���Ϊ������ģʽ 
	int iMode = 1; //1����������0������ 
	ioctlsocket(socketClient, FIONBIO, (u_long FAR*) &iMode);//���������� 



	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);


	//���ջ����� 
	char buffer[BUFFER_LENGTH];//�������ݵĻ���
							   //char buffer_send1[BUFFER_LENGTH];//�������ݵĻ���
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	//���������ݶ����ڴ� 
	std::ifstream icin;
	icin.open("../test_client.txt");

	char data[1024 * 113];
	ZeroMemory(data, sizeof(data));
	icin.read(data, 1024 * 113);
	icin.close();
	int handle;
	handle = _open("../test.txt", 0x0100); //open file for read
	long length_lvxiya = _filelength(handle); //get length of file
	for (int i = 0; i < SEQ_SIZE; ++i) {
		ack[i] = 1;
	}
	totalPacket = length_lvxiya / 1024 + 1;
	//Ϊ�˲���������������ӣ�����ʹ��  -time  ����ӷ������˻�õ�ǰʱ��
	//ʹ��  -testgbn [X] [Y]  ���� GBN  ����[X]��ʾ���ݰ���ʧ���� 
	//                    [Y]��ʾ ACK �������� 
	printTips();

	int have_time_out = 0;
	int ret;
	int recvSize = 0;
	int interval = 1;//�յ����ݰ�֮�󷵻� ack �ļ����Ĭ��Ϊ 1 ��ʾÿ�������� ack��0 ���߸�������ʾ���еĶ������� ack
	char cmd[128];
	float packetLossRatio = 0.2;  //Ĭ�ϰ���ʧ�� 0.2 
	float ackLossRatio = 0.2;  //Ĭ�� ACK ��ʧ�� 0.2 
							   //��ʱ����Ϊ������ӣ�����ѭ���������� 
	srand((unsigned)time(NULL));
	while (true) {
		gets_s(buffer);
		ret = sscanf(buffer, "%s%f%f", &cmd, &packetLossRatio, &ackLossRatio);
		//��ʼ GBN ���ԣ�ʹ�� GBN Э��ʵ�� UDP �ɿ��ļ����� 
		if (!strcmp(cmd, "-testgbn")) {
			printf("%s\n", "Begin to test GBN protocol, please don't abort the process");
			printf("The  loss  ratio  of  packet  is  %.2f,the  loss  ratio  of  ack is %.2f\n", packetLossRatio, ackLossRatio);
			int waitCount = 0;
			int stage = 0;
			BOOL b;
			unsigned char u_code;//״̬�� 
			unsigned short seq;//�������к� 
			unsigned short recvSeq;//��ȷ�ϵ�������к� 
			unsigned short waitSeq;//�ȴ������к� �����ڴ�СΪ10�����Ϊ��С��ֵ
			char buffer_1[RECV_WIND_SIZE][BUFFER_LENGTH];//���յ��Ļ���������----------------add bylvxiya
			int i_state = 0;
			for (i_state = 0; i_state < RECV_WIND_SIZE; i_state++) {
				ZeroMemory(buffer_1[i_state], sizeof(buffer_1[i_state]));
			}

			BOOL ack_send[RECV_WIND_SIZE];//ack��������ļ�¼����Ӧ1-20��ack,�տ�ʼȫΪfalse
			int success_number = 0;// �����ڳɹ����յĸ���
			for (i_state = 0; i_state < RECV_WIND_SIZE; i_state++) {//��¼��һ���ɹ�������
				ack_send[i_state] = false;
			}
			std::ofstream out_result;
			out_result.open("result.txt", std::ios::out | std::ios::trunc);
			if (!out_result.is_open()) {
				printf("�ļ���ʧ�ܣ�����\n");
				continue;
			}
			//---------------------------------
			sendto(socketClient, "-testgbn", strlen("-testgbn") + 1, 0,
				(SOCKADDR*)&addrServer, sizeof(SOCKADDR));

			while (true)
			{
				//recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
				switch (stage)
				{
				case 0://�ȴ����ֽ׶� 
					do
					{
						recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
					} while (recvSize < 0);
					u_code = (unsigned char)buffer[0];
					if ((unsigned char)buffer[0] == 205)
					{
						printf("Ready for file transmission\n");
						buffer[0] = 200;
						buffer[1] = '\0';
						sendto(socketClient, buffer, 2, 0,
							(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						stage = 1;
						recvSeq = 0;
						waitSeq = 1;
						curAck = 0;
						totalSeq = 0;
						waitCount = 0;
						curSeq = 0;
					}
					break;
				case 1://�ȴ��������ݽ׶� 
					if (seqIsAvailable() && totalSeq - RECV_WIND_SIZE <= totalPacket)
					{
						recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
						if (recvSize < 0 && have_time_out<2) {
							waitCount++;
							//20 �εȴ� ack ��ʱ�ش� 
							if (waitCount > 20)
							{
								timeoutHandler();
								waitCount = 0;
								have_time_out++;
							}
						}
						else if (have_time_out >= 2) {
							have_time_out = 0;
							buffer[1] = seq;

							buffer[0] = curSeq + 1;
							b = lossInLossRatio(ackLossRatio);
							if (b) {
								printf("The  packet from Client  of  %d and ACK for server of %d loss\n", (unsigned char)buffer[0], seq);
								break;
							}
							ack[curSeq] = 0;
							//���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������ 
							//Ϊ�򻯹��̴˴���δʵ�� 
							memcpy(&buffer[2], data + 1024 * (curSeq + (totalSeq / RECV_WIND_SIZE)*RECV_WIND_SIZE), 1024);
							printf("send a packet from Client  of  %d and ACK for server of %d \n", curSeq + 1, seq);
							sendto(socketClient, buffer, sizeof(buffer), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
							if (buffer[1] == 1) {
								printf("dsa");
							}
							++curSeq;
							curSeq %= SEQ_SIZE;
							++totalSeq;
						}
						else {
							b = lossInLossRatio(packetLossRatio);
							if (b) {
								printf("The packet from Client with a seq of %d and ACK for SERVER of %d loss\n", buffer[0], buffer[1]);
								break;
							}
							seq = (unsigned short)buffer[0];

							printf("recv packet: %d, ACK1 for SERVER:%d\n", seq, (unsigned short)buffer[1]);
							if (seq >= waitSeq && (waitSeq + RECV_WIND_SIZE > SEQ_SIZE ? true : seq < (waitSeq + RECV_WIND_SIZE)))
							{
								memcpy(buffer_1[seq - waitSeq], &buffer[2], sizeof(buffer));
								ack_send[seq - waitSeq] = true;
								int ack_s = 0;
								while (ack_send[ack_s] && ack_s < RECV_WIND_SIZE) {
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
									for (int i = 0; i < RECV_WIND_SIZE; i++) {
										if (ack_s + i < RECV_WIND_SIZE)
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
							//ע����ʱack--(unsigned short)buffer[1];�Ļ�Ҫ�����ǲ���0�������0�Ļ���ʾ�Է�û���չ����ݣ��ǲ���ackHandel�ģ�
							if ((unsigned short)buffer[1]) {
								ackHandler(buffer[1]);
								waitCount = 0;
							}
							buffer[1] = seq;

							buffer[0] = curSeq + 1;
							b = lossInLossRatio(ackLossRatio);
							if (b) {
								printf("The  packet from Client  of  %d and ACK for server of %d loss\n", (unsigned char)buffer[0], seq);
								break;
							}
							ack[curSeq] = 0;
							//���ݷ��͵Ĺ�����Ӧ���ж��Ƿ������ 
							//Ϊ�򻯹��̴˴���δʵ�� 
							memcpy(&buffer[2], data + 1024 * (curSeq + (totalSeq / RECV_WIND_SIZE)*RECV_WIND_SIZE), 1024);
							printf("send a packet from Client  of  %d and ACK for server of %d \n", curSeq + 1, seq);
							sendto(socketClient, buffer, sizeof(buffer), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
							if (buffer[1] == 1) {
								printf("dsa");
							}
							++curSeq;
							curSeq %= SEQ_SIZE;
							++totalSeq;
							//Sleep(500);
							//break;
						}
					}
					//memcpy(buffer_send1,buffer,sizeof(buffer));

					else if (curSeq - curAck >= 0 ? curSeq - curAck <= RECV_WIND_SIZE : curSeq - curAck + SEQ_SIZE <= RECV_WIND_SIZE && totalSeq - RECV_WIND_SIZE <= totalPacket) {
						curSeq++;
						curSeq %= SEQ_SIZE;
					}
					else {
						waitCount++;
						if (waitCount > 18)
						{
							timeoutHandler();
							waitCount = 0;
						}
					}
					Sleep(500);
					break;
				}
				//Sleep(500);
			}
			out_result.close();
		}
		sendto(socketClient, buffer, sizeof(buffer), 0,
			(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		do
		{
			ret =
				recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
		} while (ret < 0);



		printf("%s\n", buffer);
		if (!strcmp(buffer, "Good bye!")) {
			break;
		}
		printTips();
	}
	//�ر��׽��� 
	closesocket(socketClient);
	WSACleanup();
	return 0;
}
