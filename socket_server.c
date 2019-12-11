#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int cli_cnt = 0;
int cli_status[4]={0,0,0,0}; //0:free,1:handle invite,2:gaming 
int cli_id[4]={-1,-1,-1,-1};
char cli_acn[4][4]={"aaa\0","bbb\0","ccc\0","ddd\0"};
char cli_pwd[4][4]={"aaa\0","bbb\0","ccc\0","ddd\0"};
int b_info[2][4] = {{0,0,0,0},{0,0,0,0}};
//struct bufferevent cli[4];  

static void server_on_read(struct bufferevent* bev,void* arg){
	struct evbuffer* input = bufferevent_get_input(bev);
	size_t len = 0;
	len = evbuffer_get_length(input);

	// read
	char* buf;
	buf = (char*)malloc(sizeof(char)*len);
	if(NULL==buf){return;}
	evbuffer_remove(input,buf,len); // clear the buffer
	evutil_socket_t fd = bufferevent_getfd(bev);  
	printf("%s",buf);
	if(strcmp(buf,"ls\n")==0){
		int login_flag=0;
		for(int i=0;i<4;i++){
			if(cli_id[i]==(int)fd){
				login_flag=1;
			}
		}
		if(login_flag){
			char idlist[1024]={0};
			for(int i=0;i<4;i++){
				if(cli_id[i]!=-1){
					strcat(idlist,cli_acn[i]);
					strcat(idlist,"\n");
				}
			}
			bufferevent_write(bev,idlist,strlen(idlist));
		}
		else{
			bufferevent_write(bev,"login first",12);	
		}
	}
	if(strncmp(buf,"acn:",4)==0){
		if(cli_cnt<4){
			char *ptr=buf+4,usr_acn[1024],usr_pwd[1024];
			int flag=0,len=0;
			while(*ptr!='\n'){
				if(strncmp(ptr,",pwd:",5)==0){
					usr_acn[len] = '\0';
					ptr = ptr + 5;
					len=0;
					flag=1;	
				}
				if(flag){
					usr_pwd[len] = *ptr;
					len++;
					ptr++;
				}else{
					usr_acn[len] = *ptr;
					len++;
					ptr++;
				}
			}
			usr_pwd[len] = '\0';
			int flag2=-1;
			for(int i=0;i<4;i++){
				if(cli_id[i]==-1){
					if(strcmp(cli_acn[i],usr_acn)==0&&strcmp(cli_pwd[i],usr_pwd)==0){
						cli_id[i] = (int)fd;
						flag2 = i;
						cli_cnt++; 
						break;
					}
				}
			}
			if(flag2!=-1){
				char str[10] = "hi ";
				bufferevent_write(bev,strcat(str,cli_acn[flag2]),7);	
			}
			else{
				bufferevent_write(bev,"fail",5);
			}
		}
		else{
			bufferevent_write(bev,"sorry\0",5);
		}
	}
	if(strncmp("invite:",buf,7)==0){
		if(buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';
		char *ptr = buf+7;
		int from = -1,target=-1;
		for(int i=0;i<4;i++){
			if(fd == cli_id[i]){
				from = i;
			}
		}
		if(from==-1){
			bufferevent_write(bev,"login first\0",12);
		}else{	
			int flag = -1;
			for(int i=0;i<4;i++){
				if(-1 != cli_id[i]&&strcmp(ptr,cli_acn[i])==0){
					target = cli_id[i];
					if(cli_status[i] !=0){
						flag = i;
					}
				}
			}
			if(flag!=-1){
				bufferevent_write(bev,"player is busy",15);
			}
			else if(target!=-1){
				char tmp[1024] = "invite msg from:";
				int tmplen = strlen(cli_acn[from])+strlen(tmp);
				cli_status[flag] = 1;
				cli_status[from] = 1;
				write(target,strcat(tmp,cli_acn[from]),tmplen);
			}else{
				bufferevent_write(bev,"player does not exist\0",21);
			}
		}

	}
	if(strncmp("answer:",buf,7)==0){
		if(buf[strlen(buf)-1]=='\n'){
			buf[strlen(buf)-1] = '\0';
		}
		char *ptr = buf+8;
		char ans = buf[7];
		int b_id = -1;
		int target,idx1,idx2;
		for(int i=0;i<4;i++){
			if(strcmp(ptr,cli_acn[i])==0){
				target = cli_id[i];
				idx1 = i;
			}
			if(fd == cli_id[i]){
				idx2 = i;
			}
		} 		
		if(ans=='y'){
			if(b_info[0][0]==0){
				b_id = 0;	
			}else{
				b_id = 1;
			}
			cli_status[idx1] = 2;
			cli_status[idx2] = 2;
			b_info[b_id][0] = 1;
			b_info[b_id][1] = target;
			b_info[b_id][2] = target;
			b_info[b_id][3] = fd;
			write(target,"start\nit's your turn\n",22);
			bufferevent_write(bev,"start\n\0",7);
		}
		else if(ans=='n'){
			cli_status[idx1] = 0;
			cli_status[idx2] = 0;
			write(target,"invite is rejected",19);
		}
	}
	else if(strstr(buf,"win")!=NULL||strncmp("at:",buf,3)==0){
		int b_id;
		if(fd == b_info[0][2]||fd == b_info[0][3]){
			b_id = 0;
		}
		else{
			b_id = 1;
		}
		if(b_info[b_id][1]!=fd){
			bufferevent_write(bev,"not your turn!",15);
		}else{
			int b_me = b_info[b_id][2];
			b_info[b_id][1] = b_info[b_id][3];
			if(b_me!=fd){
				b_me = b_info[b_id][3];
				b_info[b_id][1] = b_info[b_id][2];
			}
			write(b_info[b_id][1],buf,strlen(buf));
		}
		if(strstr(buf,"over")!=NULL){
			int p1=b_info[b_id][2],p2=b_info[b_id][3];
			for(int i=0;i<4;i++){
				b_info[b_id][i] = 0;
			}
			for(int i=0;i<4;i++){
				if(cli_id[i]==p1||cli_id[i]==p2){
					cli_status[i] = 0;
				}
			}
		}

	}
	else if(strncmp(buf,"all:",4)==0){
		int str_len = strlen(buf),flag=0;
		char *ptr = buf;
		buf[str_len-1] = '\0'; 
		ptr=ptr+4;
		for(int i=0;i<4;i++){
			if(cli_id[i]==fd){
				flag = i;
			}
		}
		printf("%s\n",cli_acn[flag]);
		char msg[1024]="msg:";
		strcat(msg,cli_acn[flag]);
		strcat(msg,":\0");
		strcat(msg,ptr);
		for(int i=0;i<4;i++){
			if(cli_id[i] != fd && cli_id[i] != -1){
				write(cli_id[i],msg,strlen(msg));
			}
		}
	}
	if(strncmp(buf,"pm",2)==0){
		char name[1024]={0},msg[1024]="(pm)",content[1024]={0},*ptr=buf;
		int name_len = 0,idx1,idx2,target;
		sscanf(ptr+3,"%s%s",name,content);
		for(int i=0;i<4;i++){
			if(strcmp(name,cli_acn[i])==0){
				target = cli_id[i];
				idx1 = i;
			}
			if(fd == cli_id[i]){
				idx2 = i;
			}
		}
		strcat(msg,cli_acn[idx2]);
		strcat(msg,":\0");
		strcat(msg,content);
		write(target,msg,strlen(msg));
	}
	else if(strncmp(buf,"logout",6)==0){
		for(int i=0;i<4;i++){
			if(cli_id[i]==fd){
				cli_id[i]=-1;
				cli_cnt --;
			}
			bufferevent_write(bev,"logout",7);
		}	
	}
	free(buf);
	buf=NULL;
	return;
}


void server_on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg)
{
	printf("accept a client : %d\n", fd);
	// set TCP_NODELAY
	if(cli_cnt<4){
		int enable = 1;
		if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable)) < 0)
			printf("Consensus-side: TCP_NODELAY SETTING ERROR!\n");
		struct event_base *base = evconnlistener_get_base(listener);
		struct bufferevent* new_buff_event = bufferevent_socket_new(base,fd,BEV_OPT_CLOSE_ON_FREE);
		bufferevent_setcb(new_buff_event,server_on_read,NULL,NULL,NULL);
		bufferevent_enable(new_buff_event,EV_READ|EV_WRITE);
	}else{
		char tmp[100]="sorry\0";
		write((int)fd,tmp,strlen(tmp));
	}
	return;
}

int main()
{
	int port = 9876;
	struct sockaddr_in my_address;
	memset(&my_address, 0, sizeof(my_address));
	my_address.sin_family = AF_INET;
	my_address.sin_addr.s_addr = htonl(0x7f000001); // 127.0.0.1
	my_address.sin_port = htons(port);

	struct event_base* base = event_base_new();
	struct evconnlistener* listener = 
		evconnlistener_new_bind(base,server_on_accept,
				NULL,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,
				(struct sockaddr*)&my_address,sizeof(my_address));

	if(!listener)
		exit(1);

	event_base_dispatch(base);
	evconnlistener_free(listener);
	event_base_free(base);

	return 0;
}

