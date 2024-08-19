#define _POSIX_C_SOURCE 200112L // POSIX 기능 활성화

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>  // for open
#include <unistd.h> // for close

// 클라이언트가 서버와 연결 설정하는 함수
int open_clientfd(char *hostname, char *port)
{
    int clientfd;
    struct addrinfo hints, *listp, *p;

    /* 가능한 서버 주소들의 리스트를 받는다 */
    memset(&hints, 0, sizeof(struct addrinfo)); // addrinfo 구조체인 hints에 있는 값들을 0으로 초기화
    // 이름이 왜 hints일까? : getaddrinfo 함수 매개변수 이름이 hints
    hints.ai_socktype = SOCK_STREAM; // TCP 소켓으로 설정
    hints.ai_flags = AI_NUMERICSERV; // serivice를 포트 번호로 해석할 것
    hints.ai_flags |= AI_ADDRCONFIG; // 시스템의 IPv4, IPv6 사용 여부 가져옴
    getaddrinfo(hostname, port, &hints, &listp);

    /* 소켓 주소 구조체의 연결 리스트를 한 번 순회하면서 연결을 시도하고, 연결되면 순회 중지 */
    // getaddrinfo로 hostname과 port에 대응되는 소켓 주소 구조체의 리스트를 얻었을 거임
    // 단일 addrinfo 구조체가 아니라 리스트를 주는 이유는,
    // 호스트 이름이 여러 개의 IP 주소에 대응될 수 있기 때문임.
    for (p = listp; p; p = p->ai_next)
    {
        // 소켓 식별자 생성
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; // 소켓을 못 만들어냈음. 다음 거 시도.

        // 서버에 연결
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break; // 연결 했음. 순회 중지.

        close(clientfd); // 연결 실패했으므로 소켓을 닫음
        // (소켓 파일 디스크립터를 닫고, 시스템 리소스를 해제한다는 뜻)
    }

    /* 메모리 해제 */
    freeaddrinfo(listp);
    if (!p) // 모든 연결이 실패했다면 (포인터가 최종적으로 NULL을 가리킨다면)
        return -1;
    else // 연결이 성공했다면
        return clientfd;
}

#define LISTENQ 1024

int open_listenfd(char *port)
{
    struct addrinfo hints, *listp, *p;
    int listenfd, optval = 1;

    /* 가능한 서버 주소들의 리스트를 받는다 */
    memset(&hints, 0, sizeof(struct addrinfo));  // addrinfo 구조체인 hints에 있는 값들을 0으로 초기화
    hints.ai_socktype = SOCK_STREAM;             // TCP 소켓으로 설정
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; // AI_PASSIVE 플래그 적용하면
    // getaddrinfo 함수는 자신이 반환하는 주소 정보를 서버 측 소켓에 사용할 수 있도록 설정함
    hints.ai_flags |= AI_NUMERICSERV; // serivice를 포트 번호로 해석할 것
    getaddrinfo(NULL, port, &hints, &listp);

    for (p = listp; p; p = p->ai_next)
    {
        /* 소켓 식별자 생성 */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; // 소켓을 못 만들어냈음. 다음 거 시도.

        /* bind() 함수 호출 시 발생 가능한 "주소가 이미 사용되고 있음" 오류 방지 */
        // "Address already in use" 오류는 주로 서버 소켓이 특정 포트에 바인딩될 때,
        // 이전 연결이 제대로 종료되지 않아 해당 포트가 아직 사용 중인 경우 발생
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

        /* Bind the descriptor to the address */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break;       // 바인드 성공하면 순회 끝
        close(listenfd); // bind 실패. 다음 거 시도.
    }

    /* 메모리 해제 */
    freeaddrinfo(listp);
    if (!p) // 아무 주소랑도 bind 못 함
        return -1;
    /* 서버 소켓을 클라 연결 요청을 받을 준비가 된 상태로 만듦 */
    // listen은 서버 소켓을 수신 대기 상태로 만듦
    // 오류 생기면 닫아버림
    if (listen(listenfd, LISTENQ) < 0)
    {
        close(listenfd);
        return -1;
    }
    return listenfd;
}
