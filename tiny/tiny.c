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

  listenfd = Open_listenfd(argv[1]);
  //무한 서버 로푸 실행
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    // 트랜잭션 수행
    doit(connfd);   // line:netp:tiny:doit
    // 자신 쪽의 연결 끝을 닫는다.
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE] , method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // read request line andz headers
  // 요청 라인 읽어들이기?
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers : \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  // tiny는 GET method만 
  if(strcasecmp(method, "GET")){
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  // parse uri from get request
  is_static = parse_uri(uri, filename, cgiargs);
  if(stat(filename, &sbuf)<0){
    clienterror(fd, filename, "404", "Not found", "Tiny couldn'y read the file");
    return;
}
// 요청이 정적컨텐츠를 위한거라면
  if(is_static){ // serve static content
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

  // build the HTTP response body
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s \r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s:%s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The tiny Web server</em>\r\n", body);
  // print the http response
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
  if(!strstr(uri, "cgi-bin")){
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
  if (Fork() == 0) { /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
    Wait(NULL); /* Parent waits for and reaps child */
} 