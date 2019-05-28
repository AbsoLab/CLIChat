#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h> 

#define BUF_SIZE 100
#define MAX_CLNT 256
#define NAME_SIZE 20

unsigned WINAPI HandleClnt(void* arg);

void SendMsg(char* msg, int len, SOCKET hClntSock);
void ErrorHandling(char* msg);

void SendErr(SOCKET hClntSock);
void SendAllow(SOCKET hClntSock);

int GetClientNum(SOCKET hClntSock);
int CheckFunc(char* msg);
void SendList(SOCKET hClntSock);
void SendWhisper(char* msg, int len, int from);

int clntCnt = 0;
HANDLE hMutex;

struct Client {
    char name[NAME_SIZE];
    SOCKET clntSocks;
} client[MAX_CLNT];

int main(int argc, char* argv[])
{
    WSADATA wsaData;
    SOCKET hServSock, hClntSock;
    SOCKADDR_IN servAdr, clntAdr;
    int clntAdrSz;
    HANDLE  hThread;

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");
    

    hMutex = CreateMutex(NULL, FALSE, NULL);
    hServSock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&servAdr, 0, sizeof(servAdr));
    servAdr.sin_family = AF_INET;
    servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAdr.sin_port = htons(atoi(argv[1]));

    if (bind(hServSock, (SOCKADDR*)& servAdr, sizeof(servAdr)) == SOCKET_ERROR)
        ErrorHandling("bind() error");
    if (listen(hServSock, 5) == SOCKET_ERROR)
        ErrorHandling("listen() error");

    while (1) {
        clntAdrSz = sizeof(clntAdr);
        hClntSock = accept(hServSock, (SOCKADDR*)& clntAdr, &clntAdrSz);

        WaitForSingleObject(hMutex, INFINITE);
        client[clntCnt++].clntSocks = hClntSock;
        ReleaseMutex(hMutex);

        hThread = (HANDLE)_beginthreadex(NULL, 0, HandleClnt,
            (void*)& hClntSock, 0, NULL);
        printf("Connected client IP: %s \n",
            inet_ntoa(clntAdr.sin_addr));
    }
    closesocket(hServSock);
    WSACleanup();
    return 0;

}

unsigned WINAPI HandleClnt(void* arg)
{
    SOCKET hClntSock = *((SOCKET*)arg);
    int strLen = 0, i;
    int num = -1, check = 0;
    char msg[BUF_SIZE];

    /*이름 설정*/
    while ((strLen = recv(hClntSock, msg, sizeof(msg), 0)) != -1) {
        msg[strLen] = '\0';

        for (i = 0; i < clntCnt; ++i) {
            if (strcmp(client[i].name, msg) == 0) {
                check = -1;
            }
        }
            
        if (check == 0) {
            WaitForSingleObject(hMutex, INFINITE);
            strcpy(client[GetClientNum(hClntSock)].name, msg);
            ReleaseMutex(hMutex);
            SendAllow(hClntSock);
            break;
        } else {
            SendErr(hClntSock);
        }
    }

    while ((strLen = recv(hClntSock, msg, sizeof(msg), 0)) != -1) {
        msg[strLen] = '\0';

        switch (CheckFunc(msg)) {
        case 1:
            SendList(hClntSock);
            break;

        case 2:
            SendWhisper(msg, strLen, hClntSock);
            break;

        default:
            SendMsg(msg, strLen, hClntSock);
            break;
        }
    }
    printf("qq");

    /*
    strLen = recv(hClntSock, msg, sizeof(msg), 0);
    msg[strLen] = '\0';

    for (i = 0; i < clntCnt; ++i) {
        if (strcmp(client[i].name, msg) == 0) {
            check = -1;
        }
    }
    if (check == -1) {

        send(hClntSock, "X", 1, 0);

    } else {

        send(hClntSock, "O", 1, 0);
        num = GetClientNum(hClntSock);
        strcpy(client[num].name, msg);

        while ((strLen = recv(hClntSock, msg, sizeof(msg), 0)) != 0) {

            msg[strLen] = '\0';
            
            
            
            switch (CheckFunc(msg)) {
            case 1:
                SendList(hClntSock);
                break;

            case 2:
                SendWhisper(msg, strLen, hClntSock);
                break;

            default:
                SendMsg(msg, strLen, hClntSock);
                break;
            }
        }
    }
    */

    WaitForSingleObject(hMutex, INFINITE);
    
    for (i = 0; i < clntCnt; i++)   // remove disconnected client
    {
        if (hClntSock  == client[i].clntSocks)
        {
            while (i++ < clntCnt - 1)
                client[i] = client[i+1];
            break;
        }
    }
    clntCnt--;
    ReleaseMutex(hMutex);
    closesocket(hClntSock);
    return 0;
}

void SendMsg(char* msg, int len, SOCKET hClntSock)   // send to all
{
    int i;
    char nameMsg[NAME_SIZE + 5 + BUF_SIZE];
    

    WaitForSingleObject(hMutex, INFINITE);
    sprintf(nameMsg, "[%s] : %s", client[GetClientNum(hClntSock)].name, msg);
    for (i = 0; i < clntCnt; i++) {
        send(client[i].clntSocks, nameMsg, strlen(nameMsg), 0);
    }
    ReleaseMutex(hMutex);
}


int GetClientNum(SOCKET hClntSock) {
    for (int i = 0; i < clntCnt; ++i) {
        if (client[i].clntSocks == hClntSock) {
            return i;
        }
    }
}

/*추가기능 확인*/
int CheckFunc(char* msg) {

    char temp_msg[BUF_SIZE] = { 0, };
    strcpy(temp_msg, msg);
    char* token = NULL;
    char delim[] = " \t\n\r";
    int result = 0;

    token = strtok(temp_msg, delim);
    if (token == NULL) result = 0;
    else if (strcmp(token, "/list") == 0) result = 1;
    else if (strcmp(token, "/to") == 0) result = 2;
    return result;
}

/*이름 중복*/
void SendErr(SOCKET hClntSock) {
    const char* err = "X";

    WaitForSingleObject(hMutex, INFINITE);
    send(hClntSock, err, strlen(err), 0);
    ReleaseMutex(hMutex);
}
void SendAllow(SOCKET hClntSock) {
    const char* err = "O";

    WaitForSingleObject(hMutex, INFINITE);
    send(hClntSock, err, strlen(err), 0);
    ReleaseMutex(hMutex);
}

/*회원 목록 보내기*/
void SendList(SOCKET hClntSock) {

    char list_str[NAME_SIZE * MAX_CLNT] = {"\0",};
    for (int i = 0; i < clntCnt; ++i) { 
        sprintf(list_str, "%s%d: %s \n", list_str, i + 1, client[i].name);
    }
    WaitForSingleObject(hMutex, INFINITE);
    send(hClntSock, list_str, strlen(list_str), 0);
    ReleaseMutex(hMutex);
}

/*귓속말 보내기*/
void SendWhisper(char* msg, int len, int from) {

}

void ErrorHandling(char* msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}