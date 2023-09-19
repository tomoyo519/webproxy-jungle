#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int clientfd);

int main() {
  printf("%s", user_agent_hdr);
  //client 요청받기
  connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
  //proxy가 client에게  받은 요청을 end server로 보내기
  // 2-1 getnameinfo, hostname port파악
  serverfd = Open_clientfd(h,p);
  //2-3 while(Rio_readlineb){} // 요청 라인, 요청헤더를 읽고 buf에 저장
  //2-4 Rio_writen(servefd, buf, len(buf))
  // 3 proxy : end server가 보낸 응답 받기 + client 로 전송
  // 3-1 헤더로 읽으면서(readlineb) body의 size정보 저장
  Rio_writen(connfd);
  if(bodysize !=0){
    srcp = malloc(bodysize);
    Rio_readnb();
    Rio_writen();
    }

  return 0;
}

void doit(int clientfd){
  char buf[MAXLINE] , method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  // ⚽️ 1. request-line 읽기 [ 🤚client -> 🧑‍🏫 proxy ]
  Rio_readinitb(&request_rio, clientfd);
  Rio_readlineb(&request_rio, request_buf, MAXLINE);
  printf("Request Headers:\n %s\n", request_buf);

  //요청 라인 parsing 을 통해 'method, uri, hostname, port, path ' 찾기
  // sscanf(*str ,*format, ...) str: 데이터를 읽어들일 문자열, format : 형식 문자열, ...가변인자 리스트: format에 해당하는 변수의 주소들
  // sscanf 함수는 주로 문자열 안에 포함된 특정 형태의 데이터 추출에 사용됨
  sscanf(request_buf, "%s %s", method, uri);
  parse_uri(uri, hostname, port, path);

  //server에 전송하기 위해 요청 라인의 형식 변경 : 'method uri version' -> 'method path HTTP/1.0'
  //sprintf(str, *format, ...) 다양한 데이터 타입을 문자열로 변환하거나, 복잡한 형태의 문자열을 생성할때
  sprintf(request_buf, "%s %s %s\r\n", method, path, "HTTP/1.0");
  //지원하지 않는 Method 인 경우 예외 처리
  //strcasecmp 는 대소문자를 구분하지 않고 두 문자열을 비교. 같으면 0을 반환. s1이 s2보다 사전적으로 앞에 오면 음수, 그 반대의 경우 양수 반환
  //c언어에서는 0은 거짓이고, 0이아닌 모든값은 참. 떄문에 아래 조건은 둘다 같지 않은 경우,
  if(strcasecmp(method, "GET") && strcasecmp(method, "HEAD")){
    clienterror(clientfd,  method, "501", "Not implemented", "tiny only do implement in case of GET method");
    return;
  }

    // ⚽️ 2. request-line 전송 [ 🧑‍🏫 proxy -> 🏢 server ]
    // 받은 요청을 end server에 보낸다.
    // end server 소켓 생성. 1번의 parse_uri 함수에서 얻은 hostname, port 로 소켓생성
    // 요청 라인과 요청 헤더를 끝까지 읽으면서 end server에 전송한다.
    // 전달 받은 헤더가 요구사항에 맞는지 확인한다.

    //서버 소켓 생성
    serverfd = Open_clientfd(hostname, port);
    if(serverfd < 0){
      clienterror(serverfd, method,"502", "Bad Gateway", " 🐸 Failed to establish connect with server");
      return;
    }
    Rio_written(serverfd, request_buf, strlen(request_buf));

    // 2-2 request header 읽기 & 전송 [ 🤚client -> 🧑‍🏫 proxy -> 🏢 server ]
    read_requesthdrs(&request_rio, request_buf, serverfd, hostname, port);
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
  strcpy(path, path_ptr);

  if(port_ptr){ //port가 있는 경우
    strncpy(port, port_ptr +1, path_ptr - port_ptr -1 );
    strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr);
  }else { // port가 없는 경우
    if(is_local_test){ // ???
      strcpy(port, "80"); // port 기본값 80으로 설정
    }else{
      strcpy(port,"8000");
    }
    strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr);
  }
}

// request header를 읽고 server에 전송하는 함수
// 필수 헤더가 없는 경우에는 필수 헤더를 추가로 전송
void  read_requesthdrs(rio_t *request_rio, void *request_buf, int serverfd, char *hostname, char *port){
  int is_host_exist;
  int is_connection_exist;
  int is_proxy_connection_exist;
  int is_user_agent_exist;

  // 소켓 디스크립터로부터 한줄 씩 데이터를 안전하게 읽어오는 역할
  Rio_readlineb(request_rio, request_buf, MAXLINE); // 첫번째 줄 읽기
  //strcmp : 두 문자열을 비교하는 역할. 동일한 경우 0을 출력함.
  while(strcmp(request_buf, "\r\n")){
    if(strstr(request_buf, "Proxy-Connection") !=NULL){
      sprintf(request_buf, "Proxy-Connection:close \r\n");
      is_proxy_connection_exist = 1;
    }else if(strstr(request_buf, "Connection") !=NULL){
      sprint(request_buf, "Connection: close \r\n");
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
  //    응답 헤더에서 파악한 content-length 를 활용해 content-length 만큼 읽고 client 에 전송할 때에도 content-length ㅁ
}