#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>    // client side program
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h> 

#define BUF_SIZE 100
#define NAME_SIZE 20

void SetName(SOCKET hSock);

unsigned WINAPI SendMsg(void* arg);
unsigned WINAPI RecvMsg(void* arg);
void ErrorHandling(const char* msg);

char msg[BUF_SIZE];

int main(int argc, char* argv[])
{
    WSADATA wsaData;
    SOCKET hSock;
    SOCKADDR_IN servAdr;
    HANDLE hSndThread, hRcvThread;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    hSock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&servAdr, 0, sizeof(servAdr));
    servAdr.sin_family = AF_INET;
    servAdr.sin_addr.s_addr = inet_addr(argv[1]);
    servAdr.sin_port = htons(atoi(argv[2]));

    if (connect(hSock, (SOCKADDR*)& servAdr, sizeof(servAdr)) == SOCKET_ERROR)
        ErrorHandling("connect() error");

    SetName(hSock);

    hSndThread = (HANDLE)_beginthreadex(NULL, 0, SendMsg, (void*)& hSock, 0, NULL);
    hRcvThread = (HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void*)& hSock, 0, NULL);

    WaitForSingleObject(hSndThread, INFINITE);
    WaitForSingleObject(hRcvThread, INFINITE);

    closesocket(hSock);
    WSACleanup();

    return 0;
}

/*이미 존재하는 이름인지 확인*/
void SetName(SOCKET hSock) {

    char receive[BUF_SIZE];

    while (1) {
        printf("이름 입력 : ");
        scanf_s("%s", msg, NAME_SIZE);
        send(hSock, msg, strlen(msg), 0);
        recv(hSock, receive, BUF_SIZE, 0);
        
        if (receive[0] == 'O') {
            break;
        }

        printf("이미 서버에 존재하는 이름입니다. 다른 이름을 입력 해 주세요. \n");
    }
    
    printf("로그인에 성공하였습니다. \n");
}

unsigned WINAPI SendMsg(void* arg)   // send thread main
{
    SOCKET hSock = *((SOCKET*)arg);
    
    while (1) {
        fgets(msg, BUF_SIZE, stdin);
        if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")) {
            closesocket(hSock);
            exit(0);
        } else if (!strcmp(msg, "\n")) {

        } else {

            send(hSock, msg, strlen(msg), 0);
        }
    }
    return 0;
}

unsigned WINAPI RecvMsg(void* arg)   // read thread main
{
    int hSock = *((SOCKET*)arg);
    char nameMsg[NAME_SIZE + 5 + BUF_SIZE];
    int strLen;
    while (1) {
        strLen = recv(hSock, nameMsg, NAME_SIZE + 5 + BUF_SIZE - 1, 0);
        if (strLen == 0)
            break;
        nameMsg[strLen] = '\0';
        fputs(nameMsg, stdout);
    }
    return 0;
}

void ErrorHandling(const char* msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}