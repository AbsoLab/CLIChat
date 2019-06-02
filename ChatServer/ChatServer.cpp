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

HANDLE hMutex;

/*쓰레드 함수*/
unsigned WINAPI HandleClnt(void* arg);

/*클라이언트 설정*/
class Client {

private:
    char name[NAME_SIZE];
    char addr[NAME_SIZE];   // 123.567.901.345 15자리
    SOCKET recently_sender;
    SOCKET sock;

public:
    char* get_name() { return name; }
    char* get_addr() { return addr; }
    SOCKET get_recently_sender() { return recently_sender; }
    SOCKET get_sock() { return sock; }

    void set_name(const char* name) { strcpy(this->name, name); }
    void set_addr(const char* addr) { strcpy(this->addr, addr); }
    void set_recently_sender(SOCKET recently_sender) { this->recently_sender = recently_sender; }
    void set_sock(SOCKET sock) { this->sock = sock; }

    void init(SOCKET sock, const char* addr) {
        name[0] = 0;
        strcpy(this->addr, addr);
        recently_sender = -1;
        this->sock = sock;
    }

} clients[MAX_CLNT];
int clntCnt = 0;                            // 현재 클라이언트 수

/*클라이언트 접근 (크리티컬 섹션으로 인해 따로 설정)*/
bool CheckName(char* name);
bool SetClientName(SOCKET sock, char* name);
bool SetRecentlySender(SOCKET sock, SOCKET recently_sender);
SOCKET GetClientSock(char* name);

/*채팅 기능*/
void SendMsg(char* msg, int len);           // 전체 메시지 전송
int CheckFunc(char* msg);                   // 특수 기능 확인
void SendList(SOCKET sock);                 // 회원 목록 전송
void SendWhisper(char* msg, int len, SOCKET sock, char* from_name);     // 귓속말 전송
void SendReply(char* msg, int len, SOCKET sock, char* from_name);       // 답장 전송
void ChangeName(char* msg, int len, SOCKET from_sock, char* from_name); // 이름 변경
void SendHelp(SOCKET sock);                 // 도움말 전송
void ErrorHandling(const char* msg);


int main(int argc, char* argv[])
{
    WSADATA wsaData;
    SOCKET hServSock, sock;
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
        sock = accept(hServSock, (SOCKADDR*)& clntAdr, &clntAdrSz);

        WaitForSingleObject(hMutex, INFINITE);
        clients[clntCnt++].init(sock, inet_ntoa(clntAdr.sin_addr));
        ReleaseMutex(hMutex);

        hThread = (HANDLE)_beginthreadex(NULL, 0, HandleClnt,
            (void*)& sock, 0, NULL);
        printf("Connected client IP: %s \n",
            inet_ntoa(clntAdr.sin_addr));
    }
    closesocket(hServSock);
    WSACleanup();
    return 0;

}

/*쓰레드 함수*/
unsigned WINAPI HandleClnt(void* arg)
{
    SOCKET sock = *((SOCKET*)arg);
    int strLen = 0, i;

    char name[NAME_SIZE];
    char msg[BUF_SIZE];
    char nameMsg[NAME_SIZE + 5 + BUF_SIZE];

    /*이름 설정*/
    while ((strLen = recv(sock, name, sizeof(name), 0)) != -1) {
        
        bool check = false;
        name[strLen] = '\0';

        check = CheckName(name);

        if (check) {
            SetClientName(sock, name);
            send(sock, "O", 1, 0);
            break;

        } else {
            send(sock, "X", 1, 0);
        }
    }

    /*채팅방 전체에 입장 알림*/
    sprintf(msg, "\n***%s님께서 채팅방에 입장하셨습니다.*** \n\n", name);
    SendMsg(msg, strlen(msg));

    sprintf(msg, "\n***채팅방에 오신걸 환영합니다. 도움말이 필요하시다면 /help를 쳐보세요.*** \n\n");
    send(sock, msg, strlen(msg), 0);
    
    while ((strLen = recv(sock, msg, sizeof(msg), 0)) != -1) {

        msg[strLen] = '\0';

        switch (CheckFunc(msg)) {
        case 1:
            SendList(sock);
            break;

        case 2:
            SendWhisper(msg, strLen, sock, name);
            break;

        case 3:
            SendReply(msg, strLen, sock, name);
            break;
            
        case 4:
            ChangeName(msg, strLen, sock, name);
            break;

        case 5:
            SendHelp(sock);
            break;

        default:
            sprintf(nameMsg, "[%s] : %s", name, msg);
            SendMsg(nameMsg, strlen(nameMsg));
            break;
        }
    }

    /*채팅방 전체에 퇴장 알림*/
    sprintf(msg, "***%s님께서 채팅방에서 나가셨습니다.*** \n", name);
    SendMsg(msg, strlen(msg));

    WaitForSingleObject(hMutex, INFINITE);

    for (i = 0; i < clntCnt; i++)   // remove disconnected client
    {
        if (sock == clients[i].get_sock())
        {
            while (i < clntCnt - 1) {
                clients[i] = clients[i + 1];
                i++;
            }
            break;
        }
    }
    clntCnt--;
    ReleaseMutex(hMutex);
    closesocket(sock);
    return 0;
}

/*전체 클라이언트에게 메시지를 보낸다.*/
void SendMsg(char* msg, int len)   // send to all
{
    int i;

    WaitForSingleObject(hMutex, INFINITE);
    for (i = 0; i < clntCnt; i++) {
        send(clients[i].get_sock(), msg, len, 0);
    }
    ReleaseMutex(hMutex);
}

/*존재하는 이름인지 확인한다.*/
bool CheckName(char* name) {

    bool result = true;

    for (int i = 0; i < clntCnt; ++i) {
        if (!strcmp(clients[i].get_name(), name)) {
            result = false;
            break;
        }
    }
    return result;
}

/*sock에 해당하는 이름을 설정한다.*/
bool SetClientName(SOCKET sock, char* name) {

    bool result = false;

    WaitForSingleObject(hMutex, INFINITE);
    for (int i = 0; i < clntCnt; ++i) {
        if (clients[i].get_sock() == sock) {

            clients[i].set_name(name);
            result = true;
            break;
        }
    }
    ReleaseMutex(hMutex);

    return result;
}

/*sock에 해당하는 최근 귓속말을 보낸 사람을 설정한다.*/
bool SetRecentlySender(SOCKET sock, SOCKET recently_sender) {

    bool result = false;

    WaitForSingleObject(hMutex, INFINITE);
    for (int i = 0; i < clntCnt; ++i) {
        if (clients[i].get_sock() == sock) {

            clients[i].set_recently_sender(recently_sender);
            result = true;
            break;
        }
    }
    ReleaseMutex(hMutex);

    return result;
}

/*name에 해당하는 socket을 불러온다.*/
SOCKET GetClientSock(char* name) {

    SOCKET sock = -1;

    WaitForSingleObject(hMutex, INFINITE);
    for (int i = 0; i < clntCnt; ++i) {
        if (!strcmp(clients[i].get_name(), name)) {
            sock = clients[i].get_sock();
            break;
        }
    }
    ReleaseMutex(hMutex);

    return sock;
}

/*sock에 해당하는 recently_sender를 불러온다.*/
SOCKET GetRecentlySender(SOCKET sock) {

    SOCKET recently_sender = -1;
    bool check = false;

    WaitForSingleObject(hMutex, INFINITE);
    for (int i = 0; i < clntCnt; ++i) {
        if (clients[i].get_sock() == sock) {
            recently_sender = clients[i].get_recently_sender();
            break;
        }
    }
    ReleaseMutex(hMutex);

    if (recently_sender != -1) {
        for (int i = 0; i < clntCnt; ++i) {
            if (clients[i].get_sock() == recently_sender) {
                check = true;
                break;
            }
        }
    }

    if (!check)
        recently_sender = -1;

    return recently_sender;
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
    else if (strcmp(token, "/r") == 0) result = 3;
    else if (strcmp(token, "/change") == 0) result = 4;
    else if (strcmp(token, "/help") == 0) result = 5;
    return result;
}

/*회원 목록 보내기*/
void SendList(SOCKET sock) {

    char list_str[NAME_SIZE * MAX_CLNT] = { "\0", };
    WaitForSingleObject(hMutex, INFINITE);
    for (int i = 0; i < clntCnt; ++i) {
        sprintf(list_str, "%s%d: %s (%s) \n", list_str, i + 1, clients[i].get_name(), clients[i].get_addr());
    }
    ReleaseMutex(hMutex);
    send(sock, list_str, strlen(list_str), 0);
}

/*귓속말 보내기*/
void SendWhisper(char* msg, int len, SOCKET from_sock, char* from_name) {

    char to_name[NAME_SIZE] = { 0, };
    char nameMsg[8 + NAME_SIZE + 5 + BUF_SIZE] = { 0, };
    char delim[] = " \t\n\r";

    const char* response = "없는 사용자입니다.\n";

    strtok(msg, delim);     // 앞부분 없애기
    strcpy(to_name, strtok(NULL, delim));

    sprintf(nameMsg, "[귓속말][%s] : %s", from_name, strtok(NULL, ""));
    SOCKET to_sock = GetClientSock(to_name);
    SetRecentlySender(to_sock, from_sock);

    if (to_sock == -1) {
        send(from_sock, response, strlen(response), 0);
    } else {
        send(to_sock, nameMsg, strlen(nameMsg), 0);
    }
}

/*귓속말 답장 보내기*/
void SendReply(char* msg, int len, SOCKET from_sock, char* from_name) {

    char nameMsg[8 + NAME_SIZE + 5 + BUF_SIZE] = { 0, };
    char* token = NULL;
    char delim[] = " \t\n\r";

    const char* response = "받은 귓속말이 없거나 사용자가 존재하지 않습니다.\n";

    strtok(msg, delim);

    sprintf(nameMsg, "[귓속말][%s] : %s", from_name, strtok(NULL, ""));
    SOCKET to_sock = GetRecentlySender(from_sock);
    SetRecentlySender(to_sock, from_sock);


    if (to_sock == -1) {
        send(from_sock, response, strlen(response), 0);
    } else {
        send(to_sock, nameMsg, strlen(nameMsg), 0);
    }
}

/*사용자 이름 변경*/
void ChangeName(char* msg, int len, SOCKET sock, char* from_name) {

    char new_name[NAME_SIZE];
    char delim[] = " \t\n\r";

    const char* response = "이미 존재하는 이름입니다. \n";

    strtok(msg, delim);
    strcpy(new_name, strtok(NULL, delim));
    
    if (CheckName(new_name)) {
        // 성공
        SetClientName(sock, new_name);
        sprintf(msg, "%s님께서 %s(으)로 이름을 변경하셨습니다. \n", from_name, new_name);
        strcpy(from_name, new_name);
        SendMsg(msg, strlen(msg));
    } else {
        //실패
        send(sock, response, strlen(response), 0);
    }
}

/*도움말 보내기*/
void SendHelp(SOCKET sock) {

    const char* help_msg = " * 귓속말 : /to [이름] [메시지] \n * 답장 : /r [메시지] \n * 회원 목록 : /list \n * 이름 변경 : /change [이름] \n";
    send(sock, help_msg, strlen(help_msg), 0);
}

/*에러 핸들링*/
void ErrorHandling(const char* msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}