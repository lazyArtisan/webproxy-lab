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
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_header(int fd, char *filename, int filesize);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

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
    doit(connfd);  // line:netp:tiny:doit // 연결 수립 후 서버에서 할 일
    Close(connfd); // line:netp:tiny:close // 할 일 다 하면 연결 끝
  }
}

/* 오류 있는지, 컨텐츠가 정적인지 동적인지 확인하고 읽기/실행 */
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* 요청 라인과 헤더 읽기 */
  rio_readinitb(&rio, fd);           // 읽기 함수에 쓰일 변수들 초기화
  rio_readlineb(&rio, buf, MAXLINE); // 버퍼에 들어온 것들 읽기
  printf("request headers:\n");
  printf("%s", buf);                             // 요청 헤더 출력
  sscanf(buf, "%s %s %s", method, uri, version); // 버퍼에서 데이터 읽고 method, uri, version에 저장
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) // GET 요청인지 확인
  {
    clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method"); // 아니면 꺼지라고 함
    return;
  }
  read_requesthdrs(&rio);

  /* URI에서 데이터 추출  */
  is_static = parse_uri(uri, filename, cgiargs); // URI에서 데이터 추출
  if (stat(filename, &sbuf) < 0)                 // 파일 읽어서 버퍼에 넣기
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file"); // 오류 나면 끝내기
    return;
  }

  if (!strcasecmp(method, "HEAD")) // HEAD 요청이라면
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_header(fd, filename, sbuf.st_size);
    return;
  }

  if (is_static) // 정적 컨텐츠였다면
  {
    /* 일반 파일인지, 실행 권한이 있는지 확인 */
    // S_ISREG : st_mode 값이 일반 파일(regular file)인지 확인하는 매크로
    // sbuf에는 파일의 메타데이터가 들어가 있음. 파일의 권한, 파일 유형에 대한 정보를 비트 플래그로 저장함.
    // S_IRUSR : 사용자가 파일을 읽을 수 있는 권한을 확인하기 위한 상수
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  else // 동적 컨텐츠였다면
  {
    /* 일반 파일인지, 실행 권한이 있는지 확인 */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
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

  rio_readlineb(rp, buf, MAXLINE); // rp에 있는걸 한 줄 읽어서 buf에 넣음
  while(strcmp(buf, "\r\n")) // 버퍼에 빈줄이 나올 때까지 반복
  {
    rio_readlineb(rp, buf, MAXLINE); // rp를 한 줄 읽어서 buf에 넣음
    printf("%s", buf);
  }
  return;
}

/* uri에 담긴 정보 추출 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) // 정적 컨텐츠를 요청했다면
  {
    strcpy(cgiargs, ""); // 초기화
    strcpy(filename, "."); // .으로 초기화
    strcat(filename, uri); // uri에서 요청하는 디렉토리로 감
    if (uri[strlen(uri)-1] == '/') // uri 마지막 문자가 / 이면
      strcat(filename, "home.html"); // home.html 꺼낸다
    return 1;
  }
  else // 동적 컨텐츠를 요청했다면
  {
    ptr = index(uri, '?'); // ? 뒤에는 프로그램 매개변수들이 나옴
    if (ptr) // 매개변수가 있으면
    {
      strcpy(cgiargs, ptr + 1); // cgi 매개변수 담는 곳에 넣어준다
      *ptr = '\0'; // 넣어주고 난 후엔 매개변수 정보들 삭제함
    }
    else // 매개변수가 없으면
      strcpy(cgiargs, ""); // 매개변수 없다고 초기화시키기
    strcpy(filename, "."); // filename에 . 넣고
    strcat(filename, uri); // .uri 만들기
    return 0;
  }
}

/* 헤더 전송 */
void serve_header(int fd, char *filename, int filesize)
{
  int srcfd; // 파일 식별자 저장
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* 응답 헤더를 클라에게 보낸다 */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
  sprintf(buf + strlen(buf), "Connection: close\r\n");
  sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
  sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype); // 두 개의 개행이 중요합니다
  rio_writen(fd, buf, strlen(buf)); // 클라이언트로 응답 헤더 전송

  printf("Response headers:\n"); // 디버깅을 위해 헤더 출력
  printf("%s", buf); 
}

/* 정적 컨텐츠 전송 */
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd; // 파일 식별자 저장
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* 응답 헤더를 클라에게 보낸다 */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
  sprintf(buf + strlen(buf), "Connection: close\r\n");
  sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
  sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype); // 두 개의 개행이 중요합니다
  rio_writen(fd, buf, strlen(buf)); // 클라이언트로 응답 헤더 전송

  printf("Response headers:\n"); // 디버깅을 위해 헤더 출력
  printf("%s", buf); 

  /* 응답 body를 클라에 보낸다 */
  srcfd = open(filename, O_RDONLY, 0); // 파일을 읽기 전용 ('O_RDONLY')로 연다
  srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  // 파일 내용을 메모리에 매핑, srcp는 매핑된 메모리의 시작 주소
  // PROT_READ: 매핑된 메모리 영역을 읽기 전용으로 설정
  // MAP_PRIVATE: 메모리의 변경 사항이 파일에 반영되지 않고, 다른 프로세스와 공유되지 않음
  // srcp = malloc(filesize);
  // rio_readn(srcfd, srcp, filesize);  
  close(srcfd); // 파일 식별자 닫기
  rio_writen(fd, srcp, filesize); // 클라에게 파일 데이터 전송
  munmap(srcp, filesize); // 할당했던 메모리 반환
}

/* 해당 파일의 MIME 타입 반환 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filename, ".image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

/* 동적 컨텐츠 전송 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* HTTP 응답의 첫 부분 클라에 전송 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  rio_writen(fd, buf, strlen(buf)); 
  sprintf(buf, "Server: Tiny Web Server\r\n");
  rio_writen(fd, buf, strlen(buf)); 

  if (fork() == 0) // 자식 프로세스라면 0을 반환함
  {
    setenv("QUERY_STRING", cgiargs, 1); // URL에서 전송된 쿼리 문자열을 QUERY_STRING에 설정
    dup2(fd, STDOUT_FILENO); // 자식이 출력하는거 전부 클라로 감 (클라 소켓 식별자를 표준 출력에 복사)
    execve(filename, emptylist, environ); // CGI 프로그램 실행
  }
  wait(NULL); // 자식이 꺼질 때 까지 부모가 기다려줌
}