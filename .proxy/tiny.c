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
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  // listen socket open, 인자로 포트번호를 넘겨줌
  // Open_listenfd 는 요청받은 준비가 된 듣기 식별자를 리턴(listenfd)
  listenfd = Open_listenfd(argv[1]);
  //무한 서버 로푸 실행 : 뭔가 입력이 있어야 Doit 이 실행되는것임.
  while (1) {
    // accept 
    // accept 함수 인자에 넣기 위한 주소 길이 계산
    clientlen = sizeof(clientaddr);
    // 반복적으로 연결 요청을 접수
    //accept 함수(듣기 식별자, 소켓 주소 구조체의 주소, 주소의 길이)
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    // getnameinfo함수: 소켓주소구조체를 호스트주소, 포트 번호로 변환,
    // getaddrinfo 함수 = 호스트이름 : 호스트 주소, 서비스이름: 포트번호의 스트링 표시를 소켓 주소 구조체로 변환하는 함수
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    // 트랜잭션 수행
    doit(connfd);   // line:netp:tiny:doit
    // 자신 쪽의 연결 끝을 닫는다.
    Close(connfd);  // line:netp:tiny:close
  }
}

// 한개의 HTTP트랜잭션을 처리
void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE] , method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // read request line andz headers
  // 요청 라인 읽어들이기?
  //req line, headers 읽기
  // &rio 주소를 가지는 읽기버퍼와 식별자 connfd 연결
  Rio_readinitb(&rio, fd);
  // 버퍼에서 읽은 것이 담겨있음
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers : \n");
  // get http 1.1
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  // tiny는 GET method만 
  if(strcasecmp(method, "GET")){
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  // parse uri from get request
  // GET req로 부터 URI parse
  //uri 는 인터넷에서 특정 자원을 고유하게 식별하고 위치를 지정하는 문자열.
  //URI 를 parse 해서 파일이름, 비어있을 수 있는 cgi 인자 스트링으로 분석
  // CGI = common gateway interface의 약자로, 웹 서버와 독립적인 프로그램(일반적으로 서버 측 스크립트 또는 프로그램)
  // 사이에서 정보를 주고 받는 방법을 정의한 표준. 
  //CGI 프로그램은 웹서버가 클라이언트의 요청에 따라 실행하며, 이를 통해 동적인 컨텐츠를 생성할 수 있음.
  // CGI 단점 = 각 요청마다 새로운 프로세스가 생성된다. 많은 요청이 발생할때 성능문제
  // 정적 동적 컨텐츠인지 판단
  is_static = parse_uri(uri, filename, cgiargs);
  //file이 disk에 없다면 error return
  //stat은 파일 정보를 불러오고 sbuf에 내용을 적어줌. ok -> 0, error -> -1
  if(stat(filename, &sbuf)<0){
    clienterror(fd, filename, "404", "Not found", "Tiny couldn'y read the file");
    return;
}
// 요청이 정적컨텐츠를 위한거라면
// 정적 컨텐츠 = 웹서버에서 사용자의 요청에 따라 변경되지 않고 그대로 제공되는 콘텐츠. html, css, js파일, 이미지, 비디오
  if(is_static){ // serve static content
  //권한검사
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    //권한이 있다면 정적 컨텐츠를 클라이언트에 게 제공한다.
    serve_static(fd,filename, sbuf.st_size);
    // 요청이 동적 컨텐츠를 위한거라면
  }
    else{ // serve dynamic conent
      if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR * sbuf.st_mode)){
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
        return;
      }
      // 이 파일이 실행가능하다면 진행해서 동적 컨텐츠를 제공한다.
      serve_dynamic(fd, filename, cgiargs);
    }
  }

// 많은 에러처리가 생략되어있지만, 명백한 오류에 대해서는 체크후 클라이언트에 보고함
//html 응답은 바디에서 컨텐츠의 크기와 타입을 나타내야 한다.

void clienterror(int fd, char *cause, char *errnum, char*shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];
  // 브라우저 사용자에게 에러를 설명하는 res 본체에 HTML도 같이 보낸다.
  // build the HTTP response body

  // sprint(char *str, const char* format, ...);
  // sprint(str, "%d", 10); str문자열에는 문자열 "10"이 저장
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s \r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s:%s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The tiny Web server</em>\r\n", body);
  // print the http response

  //fprintf(fp, "%d", 10) ; fp가 가리키는 파일에다가 문자열 "10"이 작성
  sprintf(buf,"HTTP/1.0 %s %s \r\n", errnum, shortmsg );
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type : text/html \r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));


}
// tiny는 요청 헤더 내의 어떤 정보도 사용하지 않는다. 이 함수를 호출해서 읽고 무시한다.
// 요청헤더를 읽고 무시한다.
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char*cgiargs){
  char *ptr;

//static content
//strstr : str1에서 str2와 일치하는 문자열이 있는지 확인하는 함수. 있다면 해당 위치의 포인터를 return 한다. 찾지 못하면 NULL
  if(!strstr(uri, "cgi-bin")){
    // 문자열 전체를 dest로 복사하는 함수. 문자열 끝('\0')까지 복사함.
    
    strcpy(cgiargs,"");
    strcpy(filename, ".");
    strcat(filename, uri);
    if(uri[strlen(uri)-1] == '/'){
      strcat(filename, "home.html");
    }
    return 1;

  }
  //dynamic content
  else{
    ptr = index(uri, '?');
    if(ptr){
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else{
    strcpy(cgiargs, "");
  }
  strcpy(filename, ".");
  strcat(filename, uri);
  return 0;
  }
  
}

void serve_static(int fd, char *filename, int filesize) {
int srcfd;
 char *srcp, filetype[MAXLINE], buf[MAXBUF];

 /* Send response headers to client */
 get_filetype(filename, filetype);
 sprintf(buf, "HTTP/1.0 200 OK\r\n");
 sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
 sprintf(buf, "%sConnection: close\r\n", buf);
 sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
 sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
 Rio_writen(fd, buf, strlen(buf));
 printf("Response headers:\n");
 printf("%s", buf);

 /* Send response body to client */
 srcfd = Open(filename, O_RDONLY, 0); // line:netp:servestatic:open
//  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
 scrp = (char *)Malloc(filesize);
 Rio_readn(srcfd, srcp, filesize);
 Close(srcfd);
 Rio_writen(fd, srcp, filesize);
//  Munmap(srcp, filesize);dcc
 free(srcp);
 }

 /*
 * get_filetype - Derive file type from filename
 */
 void get_filetype(char *filename, char *filetype)
 {
 if (strstr(filename, ".html"))
 strcpy(filetype, "text/html");
 else if (strstr(filename, ".gif"))
 strcpy(filetype, "image/gif");
 else if (strstr(filename, ".png"))
 strcpy(filetype, "image/png");
 else if (strstr(filename, ".jpg"))
 strcpy(filetype, "image/jpeg");
 else if(strstr(filename,".mp4"))
 strcpy(filetype, "video/ogg");
 else 
 strcpy(filetype, "text/plain");
 }

void serve_dynamic(int fd, char *filename, char *cgiargs) 
  {
  char buf[MAXLINE], *emptylist[] = { NULL };
/* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  if (Fork() == 0) { /* Child */ // 동적 콘텐츠를 실행하고 결과를 return 할 자식 프로세스를 포크
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  //자식 프로세스가 실행되고 결과를 출력하고 종료될때까지 기다림
    Wait(NULL); /* Parent waits for and reaps child */
} 