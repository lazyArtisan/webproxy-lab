#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd; // 클라 소켓 디스크립터
    char *host, *port, buf[MAXLINE];
    // host : 서버 호스트 이름 또는 IP 주소 저장하는 변수의 포인터
    // port : 서버 포트 주소 저장하는 변수의 포인터
    // buf[MAXLINE] : 서버로 보낼 데이터와 서버에서 받은 데이터를 저장할 버퍼
    rio_t rio; // 구조체. 버퍼링된 입출력 처리.

    if (argc != 3) // 인자 3개 아니면 종료
    {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1]; // 명령줄에서 입력받은 host
    port = argv[2]; // 명령줄에서 입력받은 port

    clientfd = open_clientfd(host, port);
    // 주어진 호스트와 포트로 서버와의 연결을 설정하고, 해당 소켓 디스크립터를 반환
    rio_readinitb(&rio, clientfd);
    // rio_t 구조체를 초기화하여 소켓 디스크립터를 이용한 버퍼링된 읽기 작업을 준비

    while (fgets(buf, MAXLINE, stdin) != NULL)
    // 사용자가 표준 입력(stdin)으로 입력한 텍스트를 읽어와 buf에 저장
    // 루프는 fgets가 EOF 표준 입력(Ctrl+D 혹은 파일 텍스트 소진) 만나면 종료
    {
        rio_writen(clientfd, buf, strlen(buf));
        // 버퍼에 저장된 데이터를 서버로 전송
        // 소켓에 데이터를 쓰는 것은 데이터를 서버로 전송하는 것과 같음
        rio_readlineb(&rio, buf, MAXLINE);
        // 서버로부터 응답을 읽어와 buf에 저장
        fputs(buf, stdout);
        // 서버의 응답을 표준 출력(stdout)에 출력
    }
    close(clientfd);
    // 루프가 종료하고 클라이언트가 식별자를 닫으면
    // 서버로 EOF라는 통지가 전송됨

    // 열었었던 식별자들은 프로세스 종료할 때 커널이 자동으로 닫아주지만,
    // 열었던 모든 식별자들을 명시적으로 닫아주는게 올바른 습관이라고 함.
    exit(0);
}