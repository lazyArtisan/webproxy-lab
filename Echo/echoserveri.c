#include "csapp.h"

void echo(int connfd);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    // listenfd : 클라 연결 요청 대기하는 소켓 식별자
    // connfd : 클라와 연결되면 생성되는 소켓 식별자. 이걸로 데이터 주고 받음.
    socklen_t clientlen;                // 클라 주소 구조체 크기
    struct sockaddr_storage clientaddr; // IPv4, IPv6 주소 모두 담을 수 있는 구조체로 선언
    char client_hostname[MAXLINE], client_port[MAXLINE];
    // 각각 클라 호스트 이름, 클라 포트 번호 저장할 버퍼

    if (argc != 2) // 인수가 2개 아니면 종료
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = open_listenfd(argv[1]); // 듣기 식별자를 연다
    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        // 클라 주소 정보를 담을 구조체의 크기를 초기화
        connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
        // 클라의 연결 요청을 수락하면 소켓 식별자 connfd를 얻음
        getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        // 클라 호스트 이름과 포트 번호 알아내서 client_hostname, client_port에 저장
        printf("Connected to (%s %s)\n", client_hostname, client_port);
        echo(connfd); // 클라가 보낸 데이터 그대로 돌려줌
        close(connfd);
    }
    exit(0); // 무한 루프라 도달 안 하는데 오류를 대비해 넣어놓음
}