/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p1, *p2;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* 두 인수를 추출한다 */
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    p1 = strchr(buf, '&'); // &가 있는 위치를 찾는다
    *p1 = '\0'; // 널 문자(문자열의 끝)를 & 자리에 넣어준다
    strcpy(arg1, buf); // buf의 첫 문자열을 arg1에 넣는다
    p2 = strchr(arg1, '='); // 첫번째 문자열에서 = 찾기
    strcpy(arg1, p2 + 1); // = 뒤에 있는 첫 번째 인수 추출
    strcpy(arg2, p1 + 3); // buf의 두 번째 문자열을 arg2에 넣는다
    n1 = atoi(arg1); // arg1에 있는 걸 숫자로 바꾼다
    n2 = atoi(arg2); // arg2도 바꾼다
  }

  /* 응답 body 만들기 */
  sprintf(content, "QUERY_STRING=%s", buf); // 첫 번째 문자열 작성
  sprintf(content + strlen(content), "Welcome to add.com: "); // 이어서 문자열 추가
  sprintf(content + strlen(content), "THE Internet addition portal.\r\n<p>"); // 추가
  sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>", n1, n2, n1+n2); // 더한 결과 추가
  sprintf(content + strlen(content), "Thanks for visiting!\r\n"); // 마지막 메시지 추가

  /* HTTP 응답을 만든다 */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
