/*************************************************************************** * * Copyright (c) 2016 Lubanr.com All Rights Reserved * **************************************************************************/



/** * @file client.c * @date 2016/11/22 18:40:52 * @version $Revision$ * @brief * **/
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#ifndef WIN32
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
# include <arpa/inet.h>
# endif
#include <sys/socket.h>
#endif

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#define EVENT_BASE_SOCKET 0

static const int PORT = 9876;
const char* server_ip = "127.0.0.1";
char user[100]={0};
int status = 0;//1:o 2:x
int player2 = 0;
int board[3][3]={{0,0,0},{0,0,0},{0,0,0}};
int chi_cnt = 0;
int turn = 0; //1:ok 2:not
int mute_all = 0;
/*事件处理回调函数*/
int check_table[8][3] ={{1,2,3},{1,4,7},{1,5,9},{2,5,8},{3,5,7},{3,6,9},{4,5,6},{7,8,9}};
int check_win(int a[3][3]){
	int my_pos[10]={0,0,0,0,0,0,0,0,0,0};
	int id = 0;
	for(int i=0;i<3;i++){
		for(int j=0;j<3;j++){
			id++;
			if(a[i][j] == status){
				my_pos[id] = 1;	
			}	
		}
	}
	for(int i=0;i<8;i++){
		int first=check_table[i][0],second=check_table[i][1],third=check_table[i][2];
		if( my_pos[first]==1&& my_pos[second]==1 && my_pos[third]==1 ){
			return 1;
		}
	}
	return 0;
}

void print_board(int a[3][3]){
	for(int i=0;i<3;i++){
		for(int j=0;j<3;j++){
			if(board[i][j] == 1)
				printf("o");
			if(board[i][j] == 2)
				printf("x");
			if(board[i][j] == 0)
				printf(" ");
			if(j!=2){
				printf("|");
			}else{
				printf("\n");
			}
		}
		if(i!=2){
			printf("------\n");
		}
	}
}

void event_cb(struct bufferevent* bev,short events,void* ptr)
{
	if(events & BEV_EVENT_CONNECTED)//连接建立成功
	{
		printf("connected to server successed!\n");
	}
	else if(events & BEV_EVENT_ERROR)
	{
		printf("connect error happened!\n");
	}
}

void read_cb(struct bufferevent *bev, void *ctx)
{
	struct evbuffer* input = bufferevent_get_input(bev);
	size_t len = 0;
	len = evbuffer_get_length(input);
	// read
	char* buf;
	buf = (char*)malloc(sizeof(char)*len);
	if(NULL==buf){return;}
	evbuffer_remove(input,buf,len);
	if(strcmp(buf,"sorry\0")==0){
		printf("plz wait a minute\n");
		exit(1);
	}
	else if(strncmp(buf,"logout",6)==0){
		memset(user,'\0',sizeof(char));
		printf("logout success!\n");
	}
	else if(strncmp(buf,"hi ",3)==0){
		char *ptr = buf+3;
		strcpy(user,ptr);
		printf("server:\n%s\r\n",buf);
	}
	else if(strncmp(buf,"invite msg from:",16)==0){
		printf("server:%s\n",buf);
		char ans,*ptr,str[1024]="answer:\0";
		if(buf[len-1]=='\n'){
			buf[len-1] = '\0';
		}
		ptr = buf+16;
		scanf("%c",&ans);
		str[7] = ans;
		str[8] = '\0';
		strcat(str,ptr);
		bufferevent_write(bev,str,strlen(str));	 
	}
	else if(strncmp(buf,"msg:",4)==0){
		if(!mute_all){
			char *ptr = buf;
			ptr = ptr+4;
			printf("%s\n",ptr);
		}
	}
	else if(strncmp("(pm)",buf,4)==0&&mute_all==0){
		if(!mute_all)
			printf("%s\n",buf);
	}
	else if(strstr(buf,"win")!=NULL){
		printf("you win!\nleave game\n");
		player2 = 0;
		status = 0;
		turn = 0;
		for(int i=0;i<3;i++){
			for(int j=0;j<3;j++){ 
				board[i][j] = 0;
			}
		}
		chi_cnt = 0;
	}
	else if(strncmp(buf,"at:",3)==0){
		char *ptr = buf+7;
		int x = atoi(buf+3),y=atoi(buf+5);
		turn = 1;
		board[x-1][y-1] = player2;
		printf("%s\n",buf);
		if(strstr(buf,"lose")!=NULL){
			print_board(board);
			printf("\nyou lose\nleave game\n");
			player2 = 0;
			status = 0;
			turn = 0;
			for(int i=0;i<3;i++){
				for(int j=0;j<3;j++){ 
					board[i][j] = 0;
				}
			}
			chi_cnt = 0;
			return;

		}
		else if(strstr(buf,"no one win")!=NULL){
			print_board(board);
			printf("\nno one win the game\nleave game\n");
			player2 = 0;
			status = 0;
			turn = 0;
			for(int i=0;i<3;i++){
				for(int j=0;j<3;j++){
					board[i][j] = 0;
				}
			}
			chi_cnt = 0;
			return;
		}
		chi_cnt++;
		printf("your turn\n");
		print_board(board);
	}
	else{
		if(strncmp(buf,"start",5)==0){
			if(strlen(buf)<8){
				status = 2;
				player2 = 1;
				turn = 2;
			}
			else{
				status = 1;
				player2 = 2;
				turn = 1;
			}
		} 
		printf("server:\n%s\r\n",buf);
	}
}

int tcp_connect_server(const char* server_ip,int port)
{
	struct sockaddr_in server_addr;
	int status = -1;
	int sockfd;

	server_addr.sin_family = AF_INET;  
	server_addr.sin_port = htons(port);  
	status = inet_aton(server_ip, &server_addr.sin_addr);
	if(0 == status)
	{
		errno = EINVAL;
		return -1;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);  
	if( sockfd == -1 )  
		return sockfd;  


	status = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr) );  

	if( status == -1 )  
	{
		close(sockfd);
		return -1;  
	}  

	evutil_make_socket_nonblocking(sockfd);

	return sockfd;
}

void cmd_msg_cb(int fd, short events, void *arg)
{
	char msg[1024];
	int ret = read(fd,msg,sizeof(msg));
	if(ret < 0 )
	{
		perror("read failed");
		exit(1);
	}

	struct bufferevent* bev = (struct bufferevent*)arg;

	msg[ret] = '\0';
	if(strcmp(msg,"ff\n")!=0&&strncmp(msg,"at:",3)!=0&&status!=0){
		printf("you are in the game!\n");
		return;
	}
	if(strncmp(msg,"acn:",4)==0){
		if(strlen(user)){
			printf("system:plz logout first\n");
			return;
		}
	}
	if(strcmp(msg,"logout")==0){
		if(strlen(user)==0){
			printf("u r not login\n");
			return;
		}
	}
	if(strncmp(msg,"at:",3)==0){
		int flag=0;
		int x = atoi(msg+3),y=atoi(msg+5);
		if(turn!=1){
			printf("forbidden\n");
			return;
		}
		if( x<1 || x>3 || y<1 || y>3 ){
			printf("forbidden!\n");
			return;
		} 
		else if(board[x-1][y-1]!=0){
			printf("forbidden!\n");
			return;
		}
		else{
			board[x-1][y-1] = status;
			turn = 2;
			chi_cnt++;
			if(check_win(board)){
				if(msg[strlen(msg)-1] == '\n')
					msg[strlen(msg)-1] = '\0';
				flag=1;
				print_board(board);
				printf("you win\n");
				player2 = 0;
				status = 0;
				turn = 0;
				for(int i=0;i<3;i++){
					for(int j=0;j<3;j++){ 
						board[i][j] = 0;
					}
				}
				chi_cnt = 0;
				strcat(msg,"you lose!over\n");
				//printf("%s\n",msg);
			}
			else if(chi_cnt == 9){
				if(msg[strlen(msg)-1] == '\n')
					msg[strlen(msg)-1] = '\0';
				flag=1;
				print_board(board);
				printf("no one win the game!\n");
				strcat(msg,"no one win the game!over\n");
				//printf("%s\n",msg);
				player2 = 0;
				status = 0;
				turn = 0;
				for(int i=0;i<3;i++){
					for(int j=0;j<3;j++){
						board[i][j] = 0;
					}
				}
				chi_cnt = 0;
			}
		}
		if(flag==0)
			print_board(board);
	}
	if(strcmp("mute\n",msg)==0){
		mute_all = 1;
	}
	if(strcmp("unmute\n",msg)==0){
		mute_all = 0;
	}
	if(strcmp("ff\n",msg)==0){
		if(status){
			printf("you lose!\n");
			strcat(msg,"you win the game!over\n");
			//printf("%s\n",msg);
			player2 = 0;
			status = 0;
			turn = 0;
			for(int i=0;i<3;i++){
				for(int j=0;j<3;j++){
					board[i][j] = 0;
				}
			}
			chi_cnt = 0;

		}
	}
	//把终端消息发给服务器段
	bufferevent_write(bev,msg,strlen(msg));
}

int main()
{
	struct event_base* base = NULL;
	struct sockaddr_in server_addr;
	struct bufferevent *bev = NULL;
	int status;
	int sockfd;

	//申请event_base对象
	base = event_base_new();
	if(!base)
	{
		printf("event_base_new() function called error!");
	}

	//初始化server_addr
	server_addr.sin_family = AF_INET;
	server_addr.sin_port= htons(PORT);
	status = inet_aton(server_ip, &server_addr.sin_addr);

#if EVENT_BASE_SOCKET

	sockfd = tcp_connect_server(server_ip,PORT);
	bev = bufferevent_socket_new(base, sockfd,BEV_OPT_CLOSE_ON_FREE);
#else
	bev = bufferevent_socket_new(base, -1,BEV_OPT_CLOSE_ON_FREE);//第二个参数传-1,表示以后设置文件描述符

	//调用bufferevent_socket_connect函数
	bufferevent_socket_connect(bev,(struct sockaddr*)&server_addr,sizeof(server_addr));
#endif
	//监听终端的输入事件
	struct event* ev_cmd = event_new(base, STDIN_FILENO,EV_READ | EV_PERSIST, cmd_msg_cb, (void*)bev);  

	//添加终端输入事件
	event_add(ev_cmd, NULL);

	//设置bufferevent各回调函数
	bufferevent_setcb(bev,read_cb, NULL, event_cb, (void*)NULL);  

	//启用读取或者写入事件
	bufferevent_enable(bev, EV_READ | EV_PERSIST);

	//开始事件管理器循环
	event_base_dispatch(base);

	event_base_free(base);
	return 0;

}
