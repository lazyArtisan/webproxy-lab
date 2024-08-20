#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *uri_trim, char *host);
int proxy_to_server(rio_t *rp, int serverfd, char *buf);
int proxy_to_client(rio_t *rp, int clientfd, char *buf);

/* 소켓 정보 가져오게 하고 할 일 하게하는 main 함수 */
int main(int argc, char **argv)
{
  int listenfd, connfd;                  // 듣기 식별자, 연결 식별자 (서버 소켓과 클라이언트 소켓)
  char hostname[MAXLINE], port[MAXLINE]; // 클라 호스트이름, 포트 (혹은 아이피 주소, 서비스 이름)
  socklen_t clientlen;                   // 클라 주소 크기 (sockaddr 구조체 크기)
  struct sockaddr_storage clientaddr;    // 클라 주소 정보 구조체 (IPv4/IPv6 호환)

  /* 인자 맞게 썼는지 확인 */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 연결 대기 시작
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);                       // line:netp:tiny:accept // 연결 수립
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 소켓에서 클라 호스트 이름과 포트 번호 가져오기
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // 연결 수립 후 프록시 서버에서 할 일
    Close(connfd); // 할 일 다 하면 연결 끝
  }
}

/* 클라 요청에 오류 있는지 확인하고 서버로 보내기 */
void doit(int clientfd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char uri_trim[MAXLINE], host[MAXLINE];
  rio_t rio;

  

  /* 요청 라인과 헤더 읽기 */
  rio_readinitb(&rio, clientfd);     // 읽기 함수에 쓰일 변수들 초기화
  rio_readlineb(&rio, buf, MAXLINE); // 들어온 것들 읽어서 버퍼에 넣기
  printf("request headers:\n");
  printf("%s", buf);                             // 요청 라인 출력
  sscanf(buf, "%s %s %s", method, uri, version); // 버퍼에서 데이터 읽고 method, uri, version에 저장
  if (strcasecmp(method, "GET")) // GET 요청인지 확인
  {
    clienterror(clientfd, method, "501", "Not Implemented", "Tiny does not implement this method"); // 아니면 꺼지라고 함
    return;
  }
  // 여긴 출력 됨
  read_requesthdrs(&rio); // 요청 헤더 읽어오기
  // 여기 출력 안됨
  parse_uri(uri, uri_trim, host); // uri 읽고 uri_trim, host에 저장
  // 여기 출력 안 됨

  // GET http://www.cmu.edu/hub/index.html HTTP/1.1 이거를
  // GET /hub/index.html HTTP/1.0 이걸로 바꾼다
  // 앞에서 구한 host는 소켓 연결 뒤엔 필요 없는 정보  
  char server_port[4] = "8080";
  int serverfd = open_clientfd(host, server_port); // 서버와 연결
  
  proxy_to_server(&rio, serverfd, buf); // 서버에 요청을 보낸다
  rio_readlineb(&rio, buf, MAXLINE); // 서버에서 응답을 읽어와 buf에 저장
  proxy_to_client(&rio, clientfd, buf); // 클라에 요청을 보낸다
}

/* 에러 메시지 띄우는 함수 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* HTTP 응답 중 body 만들기 */
  // body를 다시 추가해서 안 넣어주면 마지막 빼고 다 사라져버림.
  // sprintf는 기존에 있던 데이터에 추가되는 방식이 아니라 덮어씌워버리기 때문.
  // 응답을 문자열 하나로 만들어서 크기를 쉽게 알 수 있게 함.
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* HTTP 응답 출력 */
  // 응답 시작 라인, 응답 헤더 뿌려준 후에 응답 body 보내줌
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  rio_writen(fd, buf, strlen(buf));
  rio_writen(fd, body, strlen(body));
}

/* 요청 헤더를 읽는 함수 */
void read_requesthdrs(rio_t *rp)
{
  // tiny 서버는 요청 헤더를 실제로 사용하진 않고, 그냥 읽기만 함.
  // rp는 클라한테 받은 데이터들 들어있는 버퍼임.

  char buf[MAXLINE];

  rio_readlineb(rp, buf, MAXLINE); // 요청 라인을 하나 제거함.
  while (strcmp(buf, "\r\n")) // 버퍼에 빈줄이 나올 때까지 반복
  {
    rio_readlineb(rp, buf, MAXLINE); // rp를 한 줄 읽어서 다시 buf에 넣음
    printf("%s", buf);
  }
  return;
}

/* uri에 담긴 정보 추출 */
void parse_uri(char *uri, char *uri_trim, char *host)
{
  // // http://www.cmu.edu/hub/index.html 이거를
  // // /hub/index.html 이걸로 바꿔서 uri_trim에 넣어야 함
  // // host에는 앞에 잘리는 부분 넣어야 함
  // char *ptr = uri;
  // int count = 0;
  
  // while ((ptr = strchr(ptr, '/')) != NULL) {
  //     count++;
  //     if (count == 3)
  //         break; // 세 번째 / 찾으면 끝
  //     ptr++; // 다음 위치로 이동
  // }

  // if(!(uri[strlen(uri)-1] == '/')) // 마지막 문자가 / 이 아니라면
  // {
  //   strcpy(uri_trim, ptr+1); // 앞에 있는 host 빼고 복사
  //   *ptr = '\0'; // 복사한 후엔 삭제
  // }

  // strcpy(host, uri); // 앞에 남아있는 host 정보 넘겨주기

  // return;
}

/* 서버로 요청 전달하는 함수 */
int proxy_to_server(rio_t *rp, int serverfd, char *buf)
{
    char *host, *port;
    rio_t rio; // 구조체. 버퍼링된 입출력 처리.

    rio_readinitb(&rio, serverfd);
    // rio_t 구조체를 초기화하여 소켓 디스크립터를 이용한 버퍼링된 읽기 작업을 준비

    while (strcmp(buf, "\r\n")) // 버퍼에 빈줄이 나올 때까지 반복
    {
      rio_writen(serverfd, buf, strlen(buf)); // 버퍼에 저장된 데이터를 서버로 전송
    }
    close(serverfd);
    exit(0);
}

/* 클라로 요청 전달하는 함수 */
int proxy_to_client(rio_t *rp, int clientfd, char *buf)
{
  rio_t rio; // 구조체. 버퍼링된 입출력 처리.

  rio_readinitb(&rio, clientfd);
  // rio_t 구조체를 초기화하여 소켓 디스크립터를 이용한 버퍼링된 읽기 작업을 준비

  while (strcmp(buf, "\r\n")) // 버퍼에 빈줄이 나올 때까지 반복
  {
    rio_writen(clientfd, buf, strlen(buf)); // 버퍼에 저장된 데이터를 서버로 전송
  }
  close(clientfd);
  exit(0);
}


