#include <stdio.h>
#include <signal.h>

#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void doit(int clientfd);
void read_requesthdrs(rio_t *request_rio, void *request_buf, int serverfd, char *hostname, char *port);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
// 테스트 환경에 따른 도메인 & 포트 지정을 위한 상수(0할당시 도메인 &포트가 고정되어 외부에서 접속 가능)
static const int is_local_test = 1;
int main(int argc, char **argv) {
  int listenfd, clientfd;
  char client_hostname[MAXLINE], client_port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }  
  //client 요청받기
  // clientfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
  //proxy가 client에게  받은 요청을 end server로 보내기
  // 2-1 getnameinfo, hostname port파악
  //2-3 while(Rio_readlineb){} // 요청 라인, 요청헤더를 읽고 buf에 저장
  //2-4 Rio_writen(servefd, buf, len(buf))
  // 3 proxy : end server가 보낸 응답 받기 + client 로 전송
  // 3-1 헤더로 읽으면서(readlineb) body의 size정보 저장
  listenfd = Open_listenfd(argv[1]); // 전달 받은 포트 번호를 사용해 수신 소켓 생성
  while (1)
  {
    clientlen = sizeof(clientaddr);
    // 클라이언트 연결 요청 수신
    clientfd = Accept(listenfd, (SA*)&clientaddr, &clientlen); 
    //getnameinfo : 소켓주소를 호스트이름과 서비스이름으로 변환하는 함수.
    //  ip주소나 포트번호를 사람이 읽을 수 있는 형식으로 변환할때 사용
    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s %s)\n", client_hostname, client_port);
    doit(clientfd);   // line:netp:tiny:doit
    // 자신 쪽의 연결 끝을 닫는다.
    Close(clientfd);  // line:netp:tiny:close
    //concurrent프록시
    
  }
  return 0;
}


void doit(int clientfd)

{
  int serverfd;
  char request_buf[MAXLINE], response_buf[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  char *response_ptr, filename[MAXLINE], cgiargs[MAXLINE] ,*srcp;
  rio_t request_rio, response_rio;

  //cgiargs : CGI 스크립트의 인자를 참조할떄 사용됨.
  // 웹서버에서 CGI 스크립트를 호출할때 클라이언트로부터 받은 요청 정보를 스크립트에 전달하기 위해 인자 사용
  // http get 요청의 경우, URL에 포함된 쿼리 문자열이 cgiargs가 됨.
  // 웹서버는 이러한 cgiargs를 CGI 스크립트에 전달하고, 스크립트는 이 정보를 사용하여 필요한 처리를
  //수행한 다음에 결과를 웹서버로 다시 반환함. 그런다음 웹서버는 이 결과를 클라이언트에게 전송함.
  // cgiargs는 웹 클라이언트와 서버 사이의 상호작용에서 중요한 역할을 함


    /* Request 1 - 요청 라인 읽기 [🙋‍♀️ Client -> 🚒 Proxy] */
  Rio_readinitb(&request_rio, clientfd);
  Rio_readlineb(&request_rio, request_buf, MAXLINE);
  printf("Request headers:\n %s\n", request_buf);

  //요청 라인 parsing 을 통해 'method, uri, hostname, port, path ' 찾기
  // sscanf(*str ,*format, ...) str: 데이터를 읽어들일 문자열, format : 형식 문자열, ...가변인자 리스트: format에 해당하는 변수의 주소들
  // sscanf 함수는 주로 문자열 안에 포함된 특정 형태의 데이터 추출에 사용됨
  sscanf(request_buf, "%s %s", method, uri);
  parse_uri(uri, hostname, port, path);
  //server에 전송하기 위해 요청 라인의 형식 변경 : 'method uri version' -> 'method path HTTP/1.0'
  //sprintf(str, *format, ...) 다양한 데이터 타입을 문자열로 변환하거나, 복잡한 형태의 문자열을 생성할때
  sprintf(request_buf, "%s %s %s\r\n", method, path, "HTTP/1.0");

  // 지원하지 않는 method인 경우 예외 처리
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(clientfd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  //Rio_readinitb : rio_t구조체를 초기화함. 그래서 이 구조체를 사용하여 안정적 입출력 작업을 수행하게 함

  //지원하지 않는 Method 인 경우 예외 처리
  //strcasecmp 는 대소문자를 구분하지 않고 두 문자열을 비교. 같으면 0을 반환. s1이 s2보다 사전적으로 앞에 오면 음수, 그 반대의 경우 양수 반환
  //c언어에서는 0은 거짓이고, 0이아닌 모든값은 참. 떄문에 아래 조건은 둘다 같지 않은 경우,
  // if(strcasecmp(method, "GET") && strcasecmp(method, "HEAD")){
  //   clienterror(clientfd,  method, "501", "Not implemented", "tiny only do implement in case of GET method");
  //   return;
  // }

    // ⚽️ 2. request-line 전송 [ 🧑‍🏫 proxy -> 🏢 server ]
    // 받은 요청을 end server에 보낸다.
    // end server 소켓 생성. 1번의 parse_uri 함수에서 얻은 hostname, port 로 소켓생성
    // 요청 라인과 요청 헤더를 끝까지 읽으면서 end server에 전송한다.
    // 전달 받은 헤더가 요구사항에 맞는지 확인한다.

    //서버 소켓 생성
  serverfd = is_local_test ? Open_clientfd(hostname, port) : Open_clientfd("3.34.134.199", port);
  if (serverfd < 0)
  {
    clienterror(serverfd, method, "502", "Bad Gateway", "📍 Failed to establish connection with the end server");
    return;
  }

      /* Request 2 - 요청 라인 전송 [🚒 Proxy -> 💻 Server] */
  Rio_writen(serverfd, request_buf, strlen(request_buf));

    // // 네트워크 디스크립터에 데이터를 안전하게 쓰는 역할
    // Rio_readinitb(&serverfd, serverfd);
  
  /* 2️⃣ Request Header 읽기 & 전송 [🙋‍♀️ Client -> 🚒 Proxy -> 💻 Server] */
  read_requesthdrs(&request_rio, request_buf, serverfd, hostname, port);


    /* Response 1 - 응답 라인 읽기 & 전송 [💻 Server -> 🚒 Proxy -> 🙋‍♀️ Client] */
  Rio_readinitb(&response_rio, serverfd);                   // 서버의 응답을 담을 버퍼 초기화
  Rio_readlineb(&response_rio, response_buf, MAXLINE);      // 응답 라인 읽기
  Rio_writen(clientfd, response_buf, strlen(response_buf)); // 클라이언트에 응답 라인 보내기

  /* 3️⃣ Response Header 읽기 & 전송 [💻 Server -> 🚒 Proxy -> 🙋‍♀️ Client] */
  int content_length;
  while (strcmp(response_buf, "\r\n"))
    //   // Rio_readlineb(rio패키지를 사용하여 초기화된 rio_t구조체의 포인터 , 읽은 데이터를 저장할 버퍼의 포인터, 읽을 수 있는 최대 바이트 수)
    //   // 파일 또는 네트워크 디스크립터로부터 한줄을 읽어 버퍼에 저장하는 역할
     
    //   //strstr 문자열 내에서 다른 문자열 찾는데 사용. 처음으로 나타내는 위치를 찾아 그 위치의 포인터를 반환함.
    {
      Rio_readlineb(&response_rio, response_buf, MAXLINE);
      if (strstr(response_buf, "Content-length")) // Response Body 수신에 사용하기 위해 Content-length 저장
          content_length = atoi(strchr(response_buf, ':') + 1);
      Rio_writen(clientfd, response_buf, strlen(response_buf));
    }

        /* Response 3 - 응답 바디 읽기 & 전송 [💻 Server -> 🚒 Proxy -> 🙋‍♀️ Client] */
    if (content_length)
    {
        srcp = malloc(content_length);
        Rio_readnb(&response_rio, srcp, content_length);
        Rio_writen(clientfd, srcp, content_length);
        free(srcp);
    }

    // // Rio_writen(fd, usrbuf, n) usrbuf로부터 최대 n바이트의 데이터를 읽어서 지정된 소켓 디스크립터로(fd)전송
    // //client에 response body 전송
    
    // free(response_ptr); // 캐싱하지 않은 경우만 메모리 반환
    // while((n = Rio_readlineb(&server_rio, buf, MAXLINE))!=0){
    //   printf("%s\n", buf);
    //   Rio_writen(fd, buf, n);
    // }

}


//uri 를 'hostname' 'port' 'path'로 파싱하는 함수
// uri 형태 : 'http://hostname:port/path' or 'http://hostname/path' : port는 optional
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
  //strstr 문자열 내에서 다른 문자열의 첫번쨰 출현을 찾는 역할
  //host_name의 시작 위치 포인터 :  '//'가 있으면 //뒤 (ptr+2) 부터, 없으면 uri 처음부터
  char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;
  //strchr
  char *port_ptr = strchr(hostname_ptr, ':'); // port 시작위치. 없으면 NULL
  char *path_ptr = strchr(hostname_ptr, '/'); // path 시작 위치 . 없으면 NULL
  //path 에 path_ptr 복사
  

    if (port_ptr != NULL) // port 있는 경우
    {
        strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1); // port 구하기
        port[path_ptr - port_ptr - 1] = '\0';
        strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr); // hostname 구하기
    }
    else // port 없는 경우
    {
        if (is_local_test)
            strcpy(port, "80"); // port는 기본 값인 80으로 설정
        else
            strcpy(port, "8000");
        strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr); // hostname 구하기
    }
    strcpy(path, path_ptr); // path 구하기
    return;
}

// request header를 읽고 server에 전송하는 함수
// 필수 헤더가 없는 경우에는 필수 헤더를 추가로 전송
void read_requesthdrs(rio_t *request_rio, void *request_buf, int serverfd, char *hostname, char *port){
  int is_host_exist = 0;
  int is_connection_exist= 0;
  int is_proxy_connection_exist= 0;
  int is_user_agent_exist= 0;

  // 소켓 디스크립터로부터 한줄 씩 데이터를 안전하게 읽어오는 역할
  Rio_readlineb(request_rio, request_buf, MAXLINE); // 첫번째 줄 읽기
  //strcmp : 두 문자열을 비교하는 역할. 동일한 경우 0을 출력함.
  while(strcmp(request_buf, "\r\n")){  // 버퍼에서 읽은 줄이 '\r\n'이 아닐 때까지 반복
    if(strstr(request_buf, "Proxy-Connection") !=NULL){
      sprintf(request_buf, "Proxy-Connection:close \r\n");
      is_proxy_connection_exist = 1;
    }else if(strstr(request_buf, "Connection") !=NULL){
      sprintf(request_buf, "Connection: close \r\n");
      is_connection_exist =1;
    }else if(strstr(request_buf, "User-Agent") != NULL){
      sprintf(request_buf, user_agent_hdr);
      is_user_agent_exist = 1;
    }else if(strstr(request_buf, "Host") != NULL){
      is_host_exist = 1;
    }
    // rio_writen : 지정된 소켓 디스크립터를 통해 데이터를 안전하게 전송하는 역할
    Rio_writen(serverfd, request_buf, strlen(request_buf)); // server에 전송
    Rio_readlineb(request_rio, request_buf, MAXLINE); // 다음 줄 읽기


  }
  //필수 헤더 미포함 시 추가로 전송
  if(!is_proxy_connection_exist){
    sprintf(request_buf, "Proxy-Connection: close\r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if( !is_connection_exist){
    sprintf(request_buf, "Connection: close \r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if(!is_user_agent_exist){
    sprintf(request_buf, user_agent_hdr);
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }

  sprintf(request_buf, "\r\n"); // 종료문
  Rio_writen(serverfd, request_buf, strlen(request_buf));
  return;

//  end server = 네트워크 통신에서 사용되는 용어.
// 데이터를 최종적으로 수신하거나 전송하는 장치. 클라이언트 또는 서버가 될 수 있음.
// 네트워크 상에서 직접 사용자와 상호작용하는 시스템
// 예를들어서, 웹브라우징을 하는 경우 웹브라우저로 실행되고 있는 컴퓨터는 클라이언트 역할을 하며
// end server로 간주될 수 있음. 웹페이지의 내용을 제공하는 웹서버도 end server로 간주된다.
  // ⚽️ 3. end server 가 보낸 응답을 받고 client에 전송한다.
  // response header 읽기
  //    1. 응답헤더를 한줄한줄 읽으면서 한줄씩 바로 client에 전송
  //    2. 응답 바디 전송에 사용하기 위해 content-length 헤더를 읽을 때는 값을 저장해둔다.
  //response body 읽기
  //    응답 바디는 이진 데이터가 포함되어 있을 수 있으므로,ㅈ한줄씩 읽지 않고, 필요한 사이즈만큼 한번에 읽는다.
  //    응답 헤더에서 파악한 content-length 를 활용해 content-length 만큼 읽고 client 에 전송할 때에도 content-length 만큼 전송한다.
  // int n;
  // while((n = Rio_readlineb(&response_rio, response_buf, MAXLINE)) > 0){
  //   Rio_writen(clientfd, response_buf, n);
  // }
  // //rio_readnb 사용
  // Rio_readnb(&response_rio, response_buf, content_length);
  // Rio_writen(clientfd, response_buf, content_length);
  // response header에서 content-length 를 파싱해서 파악하고, content-length 만큼 읽고 전송한다.
  // readline이 아닌 read 함수를 사용하면 바이트 단위로 데이터를 읽거나 쓸 수 있다.
  //read 함수는 문자열의 끝을 나타내는 NULL 종료 문자열이 없는 이진 데이터를 처리할 수 있도록 설계 되어 있다.

}

//클라이언트에 에러를 전송하는 함수
// cause : 오류 원인, errnum: 오류 번호, shortmsg: 짧은 오류 메시지, longmsg: 긴 오류 메시지
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];

  //에러 바디 생성
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s \r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s:%s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The tiny Web server</em>\r\n", body);

  //fprintf(fp, "%d", 10) ; fp가 가리키는 파일에다가 문자열 "10"이 작성
  sprintf(buf,"HTTP/1.0 %s %s \r\n", errnum, shortmsg );
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type : text/html \r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}