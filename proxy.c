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
void *thread(void *argptr);

// 클라이언ㅌ트 소프트웨어의 정보.
// 서버는 이 정보를 통해 적절한 콘텐츠를 제공하거나, 화면을 최적화하여 보여줄 수 있다.
// firefox 브라우저를 사용하는 클라이언트구나! => 더 적절한 콘텐츠 제공 가능
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

// 테스트 환경에 따른 도메인 & 포트 지정을 위한 상수(0할당시 도메인 &포트가 고정되어 외부에서 접속 가능)
static const int is_local_test = 1;

int main(int argc, char **argv) {
  int listenfd, *clientfd;
  char client_hostname[MAXLINE], client_port[MAXLINE]; // 프록시가 요청을 받고 응답해줄 클라이언트의 Host, ip
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  // 멀티스레드 방법 ! 
  // 요청 하나하나에 개별 회선을 지정해주기.
  //malloc 함수를 사용해 개별 메모리에 회선을 넣어서 처리해주자.

  pthread_t tid; // 스레드에 부여할 tid 번호
//쓰레드(thread)는 프로그램 내에서 실행 흐름의 단위. 한 프로세스 내에서 여러개의 쓰레드를 생성하면, 각 쓰레드는 독립적으로 실행되면서 병렬처리를 가능하게 함
// 쓰레드의 특징
// 1. 자원공유 : 같은 프로세스 내의 쓰레드들은 메모리와 파일 등의 자원을 공유함. 
// 2. 독립적인 실행 흐름 : 각 쓰레드는 독립적잉ㄴ 스택을 가지며 자신만의 레지스터 상태와 프로그램 카운터를 가짐
// 3. 경량화 : 쓰레드는 프로세스에 비해 생성과 제거, 전환 등이 빠르고 경제적임.
// 쓰레드 사용시 주의점 : 동기화문제. 여러 쓰레드가 동시에 같은 데이터를 접근하거나 수정하면 데이터 불일치 문제가 발생할 수 있음.
// 데드락. 두개 이상의 스레딩이 서로 필요한 리소스를 잡고 있는 상태에서 서로 대기하는 현상
// 해결방법 : 뮤텍스 = 상호배제. 여러 스레드나 프로세스가 공유 자원에 동시에 접근하는 것을 방지하는 동기화 기법
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }  
  //sequential 구현 !
  //client 요청받기
  // clientfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
  //proxy가 client에게  받은 요청을 end server로 보내기
  // 2-1 getnameinfo, hostname port파악
  //2-3 while(Rio_readlineb){} // 요청 라인, 요청헤더를 읽고 buf에 저장
  //2-4 Rio_writen(servefd, buf, len(buf))
  // 3 proxy : end server가 보낸 응답 받기 + client 로 전송
  // 3-1 헤더로 읽으면서(readlineb) body의 size정보 저장

  // 어떤 클라이언트의 요청이 들어오면, 프록시는 클라이언트의 요청에 담겨있는 목적지 호스트와 포트를 뽑아내어
  // 목적지 서버와 clientfd를 맺고, 가운데에서 요청과 응답을 전달한다.
  listenfd = Open_listenfd(argv[1]); // 전달 받은 포트 번호를 사용해 수신 소켓 생성
  while (1)
  {
    clientlen = sizeof(clientaddr);

     // 프록시가 서버로서 클라이언트와 맺는 파일 디스크립터(소켓 디스크립터) : 고유 식별되는 회선이자 메모리 그 자체    
    clientfd = (int *)Malloc(sizeof(int)); // 여러개의 디스크립터를 만들것이므로 덮어쓰지 못하도록 고유메모리에 할당
    *clientfd = Accept(listenfd, (SA*)&clientaddr, &clientlen); // 프록시가 서버로서 클라이언트와 맺는 파일 디스크립터( = 소켓디스크립터)
    //getnameinfo : 소켓주소를 호스트이름과 서비스이름으로 변환하는 함수.
    //  ip주소나 포트번호를 사람이 읽을 수 있는 형식으로 변환할때 사용
    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    Pthread_create(&tid, NULL, thread, clientfd); // 프록시가 중개를 시작 
    printf("Accepted connection from (%s %s)\n", client_hostname, client_port);

    // sequential 시 사용
    // doit(clientfd);   // line:netp:tiny:doit
    // 자신 쪽의 연결 끝을 닫는다.
    // Close(clientfd);  // line:netp:tiny:close

    
  }
  return 0;
}

//멀티 스레딩 서버에서 각 클라이언트 연결을 처리하는 쓰레드의 메인 함수.
void *thread(void *argptr){
  int clientfd = *((int *) argptr); // 클라이언트와 연결된 파일 디스크립터
  Pthread_detach((pthread_self())); // 현재 실행중인 쓰레드를 분리함. => 쓰레드 정리
  Free(argptr);
  doit(clientfd); // clientfd를 통해 데이터를 읽고, 응답을 보내는 등의 작업 수행
  Close(clientfd);
  return NULL;
}

//RIO = robust I/O = 안정적인 입출력 패키지 
// rio는 Registered Input/Output 라는 소켓 API 이다. 메시지를 보낼 수 있는 텍스트창으로 보면 됨.
//1. 클라이언트와의 fd를 클라이언트용 rio 에 연결(rio_readinit)
//2. 클라이언트의 요청을 한줄 읽어들여서(rio_readlineb) 메서드와 url, http버전을 얻고, url에서 목적지 호스트와 포트를 뽑아낸다.
// 3. 목적지 호스트와 포트를 가지고 서버용 fd를 생성하고, 서버용 rio 에 연결(rio_readinitb)한다.
// 4. 클라이언트가 보낸 첫줄을 이미 읽어 유실되었고, HTTP버전을 바꾸거나 추가 헤더를 붙일 필요가 있으므로,
// 5. 서버에 요청 메시지를 보낸다.(RIo-writen)
// 6. 서버응답이 오면 클라이언트에게 전달한다(rio_readnb, rio_writen)

void doit(int clientfd)

{
  int serverfd;
  char request_buf[MAXLINE], response_buf[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  char *response_ptr, filename[MAXLINE], cgiargs[MAXLINE] ,*srcp;
  rio_t request_rio, response_rio;

    /* Request 1 - 요청 라인 읽기 [🙋‍♀️ Client -> 🧑‍🏫 Proxy] */
    //브라우저에 http://www.cmu.edu/hub/index.html 과 같은 URL을 입력하면, 아래 http 요청받음
    //GET http://www.cmu.edu/hub/index.html ****HTTP/1.1
    // 이것을 파싱하면, 호스트이름 = www.cmu.edu 쿼리, path = hub/index.html
    // 프록시 서버는 www.cmu.edu와의 연결을 열고, 아래와 같은 http 요청을 보낸다.
    // GET / hu/index.html HTTP/1.0

  Rio_readinitb(&request_rio, clientfd);
  Rio_readlineb(&request_rio, request_buf, MAXLINE);
  printf("Request headers:\n %s\n", request_buf);


  // ??? 학습의 목적이 무엇인가?
  // HTTP 동작 및 소켓을 사용하여 네트워크 연결을 통신하는 프로그램 작성 방법.



  // sscanf(*str ,*format, ...) str: 데이터를 읽어들일 문자열, format : 형식 문자열, ...가변인자 리스트: format에 해당하는 변수의 주소들
  // sscanf 함수는 주로 문자열 안에 포함된 특정 형태의 데이터 추출에 사용됨 . 추출 후 request_buf에 저장
  sscanf(request_buf, "%s %s", method, uri);

  //요청 라인 parsing 을 통해 'method, uri, hostname, port, path ' 찾기
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

      /* Request 2 - 요청 라인 전송 [🧑‍🏫Proxy -> 💻 Server] */
  Rio_writen(serverfd, request_buf, strlen(request_buf));

    // // 네트워크 디스크립터에 데이터를 안전하게 쓰는 역할
    // Rio_readinitb(&serverfd, serverfd);
  
  /* 2️⃣ Request Header 읽기 & 전송 [🙋‍♀️ Client -> 🧑‍🏫Proxy -> 💻 Server] */
  read_requesthdrs(&request_rio, request_buf, serverfd, hostname, port);


    /* Response 1 - 응답 라인 읽기 & 전송 [💻 Server -> 🧑‍🏫Proxy -> 🙋‍♀️ Client] */
  Rio_readinitb(&response_rio, serverfd);                   // 서버의 응답을 담을 버퍼 초기화
  Rio_readlineb(&response_rio, response_buf, MAXLINE);      // 응답 라인 읽기
  Rio_writen(clientfd, response_buf, strlen(response_buf)); // 클라이언트에 응답 라인 보내기

  /* 3️⃣ Response Header 읽기 & 전송 [💻 Server -> 🧑‍🏫Proxy -> 🙋‍♀️ Client] */
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

        /* Response 3 - 응답 바디 읽기 & 전송 [💻 Server -> 🧑‍🏫 Proxy -> 🙋‍♀️ Client] */
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