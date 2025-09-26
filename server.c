#include"my.h"

#define REGISTER 1//注册请求
#define LOGIN 2//登录请求
#define LIST 3//获取列表请求
#define GET 4//下载文件请求
#define PUT 5//上传文件请求
#define HISTORY 6//历史记录请求
//双方的通信协议是一个结构体
typedef struct 
{
	int type;//请求的类型
	char username[30];//用户名
	char passwd[30];//用户密码
	char filename[30];//上传或者下载文件的名字
	char filedata[200];//上传或下载文件的内容
	int size;//用来记录每次实际读取和写入的字节数
}MSG;
//句柄
sqlite3 *db=NULL;//每一个函数都要操作
//自定义mysqlite3exec函数
void mySqlite3Exec(sqlite3 *db,const char *sql)
{
	int ret;
	char *errmsg=NULL;//保存数据库执行失败的原因
	ret=sqlite3_exec(db,sql,NULL,NULL,&errmsg);
	if(ret!=0)
	{
		printf("sqlite3_exec failed:%s",errmsg);
		exit(-1);
	}
}
//自定义mysqlite3gettable函数
int mySqlite3GetTable(sqlite3 *db,const char *sql)
{
	int ret;
	int row,column;
	char *errmsg=NULL;
	char **p=NULL;
	ret=sqlite3_get_table(db,sql,&p,&row,&column,&errmsg);
	if(ret!=0)
	{
		printf("sqlite3_get_table failed");
		exit(-1);
	}
	return row;//返回值是查询到的行数,行数不带表头,如果是0说明没有查询到
}
//获取系统时间
void getSystemTime(char *s)//s = systemtime
{
	time_t t;
	time(&t);//1970-1-1 0:0:0距今的秒数
	struct tm *tp = localtime(&t);//将秒数转换为中文格式的时间
	sprintf(s,"%d-%02d-%02d,%02d-%02d-%02d",tp->tm_year+1900,tp->tm_mon+1,tp->tm_mday,tp->tm_hour,tp->tm_min,tp->tm_sec);
}

//服务器处理客户端提出的注册请求
void doRegister(MSG *p,int newfd)
{
	char sql[200]={0};
	sprintf(sql,"select * from user_info where username='%s';",p->username);
	//执行数据库语句
	int row=mySqlite3GetTable(db,sql);
	//对查询到的行数做判断
	if(row>0)//说明当前注册的用户名已经存在
	{
		p->type==-1;//告诉客户端注册失败
	}
	else if(row==0)//说明当前注册的用户名不存在,可以注册
	{
		sprintf(sql,"insert into user_info values('%s','%s');",p->username,p->passwd);
		//创建一个和当前新注册的用户名的目录文件,当做此用户的个人空间
		mySqlite3Exec(db,sql);
		int ret=fork();//创建子进程
		if(ret==-1)
		{
			perror("fork failed");
			exit(-1);
		}
		else if(ret==0)
		{
			char pathname[50]={0};
			sprintf(pathname,"/home/linux/%s",p->username);
			execlp("mkdir","mkdir",pathname,NULL);//将当前的子进程替换
		}
		p->type=0;//告诉客户端注册成功
	}
	//将带有注册结果的结构体回传给服务器
	send(newfd,p,sizeof(*p),0);
}
//服务器处理客户端的登录请求
void doLogin(MSG *p,int newsockfd)
{
	char sql[200] = {0};
	char systemtime[50]={0};
	//查看用户名和密码在数据库是否存在
	sprintf(sql,"select * from user_info where username = '%s' and password = '%s';",p->username,p->passwd);
	int row = mySqlite3GetTable(db,sql);
	//对查询结果做判断
	if(row == 1)
	{
		//说明用户名和密码存在
		p->type = 0;//说明存在 告诉客户端可以登陆
		getSystemTime(systemtime);
		sprintf(sql,"insert into his_info values('%s','%s',LOGIN);",p->username,systemtime);
		mySqlite3Exec(db,sql);
	}
	else if(row == 0)
	{
		//用户名错误或者密码错误 继续区分是用户名错误还是密码错误
		sprintf(sql,"select * from user_info where username = '%s';",p->username);
		//查询用户名是否正确 
		row = mySqlite3GetTable(db,sql);
		if(row == 1)
		{
			//说明用户名是正确的 就说明是密码错误
			p->type = -2;//说明是因为密码错误导致登陆失败
		}
		else if(row == 0)
		{
			//说明用户名直接不存在 就是用户名错误
			p->type = -1;//说明是因为用户名有误导致的失败
		}
	}
	//程序执行到这里 type保存了成功与失败 以及 失败的因为
	send(newsockfd,p,sizeof(*p),0);//将结果回传给客户端
}
//处理客户端获取列表的请求
void doList(MSG *p,int newsockfd)
{
	//格式化用户目录的绝对路径
	char pathname[50] = {0};
	sprintf(pathname,"/home/linux/%s",p->username);
	//打开当前用户的个人空间
	DIR *dp = opendir(pathname);
	if(dp == NULL)
	{
		perror("opendir failed");
		exit(-1);
	}
	//打开成功以后循环读取
	struct dirent *ep = NULL;//保存每次读取出来的结果
	while((ep = readdir(dp)) != NULL)//当readdir的返回值为NULL时 说明读取结束
	{
		//隐藏文件不发给客户端
		if(ep->d_name[0] == '.')//说明是隐藏文件 不发给客户端
			continue;//跳出去 去读下一个文件名
		//说明不是隐藏文件 那么将本次读取出来的文件名放入结构体中
		strcpy(p->filename,ep->d_name);
		//将本次读取的结果给客户端
		send(newsockfd,p,sizeof(*p),0);
	}
	//说明个人空间中全部文件读取完毕
	p->type = 0;//代表读取完毕
	//将结构体额外发送一次 用于告诉客户端我已经将全部内容发送完毕
	send(newsockfd,p,sizeof(*p),0);
	//将当前用户的读取目录操作写入历史信息表
	char systemtime[50] = {0};
	char sql[200] = {0};
	getSystemTime(systemtime);
	sprintf(sql,"insert into his_info values('%s','%s','LIST');",p->username,systemtime);
	mySqlite3Exec(db,sql);
}
//处理客户端下载文件请求
void doGet(MSG *p,int newsockfd)
{
	char pathname[100]={0};
	//根据客户端传来的用户名和文件名拼接要读取文件的绝对路径
	sprintf(pathname,"/home/linux/%s/%s",p->username,p->filename);
	//以只读的方式打开文件
	int fd=open(pathname,O_RDONLY);
	if(fd==-1)
	{
		perror("open failed");
		exit(-1);
	}
	//循环读取文件内容并发送给客户端
	//-1给'\0'留位置
	while(p->size = read(fd,p->filedata,sizeof(p->filedata)-1))
	{
		send(newsockfd,p,sizeof(*p),0);
		usleep(1000);//1ms 如果是会员 扩展功能
	}
	//读取完毕
	p->type=0;
	//额外发送　让客户端知道服务器发送完毕
	send(newsockfd,p,sizeof(*p),0);
	//将当前用户的下载文件操作写入历史信息表
	char sql[200]={0};
	char systemtime[50]={0};
	sprintf(sql,"insert into his_info values('%s','%s','PUTFILE');",p->username,systemtime);
	mySqlite3Exec(db,sql);
}
//处理客户端上传文件请求
void *doPut(MSG *p,int newsockfd)
{
	char pathname[100]={0};
	//格式化打开文件的绝对路径
	sprintf(pathname,"/home/linux/%s/%s",p->username,p->filename);
	//打开文件　以可写+不存在创建+存在就清空
	int fd=open(pathname,O_WRONLY|O_CREAT|O_TRUNC,0666);
	if(fd==-1)
	{
		perror("open failed");
		exit(-1);
	}
	//先将文件的内容写入一次
	write(fd,p->filedata,p->size);
	//循环读取客户端发来的文件内容并写入文件中
	while(1)
	{
		recv(newsockfd,p,sizeof(*p),0);
		if(p->type==0)
		{
			close(fd);
			exit(-1);
		}
		write(fd,p->filedata,p->size);
	}
	//将上传文件的操作写入历史记录表中
	char sql[200]={0};
	char systemtime[50]={0};
	getSystemTime(systemtime);
	sprintf(sql,"insert into his_info values('%s','%s',PUTFILE);",p->username,systemtime);
	mySqlite3Exec(db,sql);
}
//服务器处理客户端历史记录功能的请求
void *doHistory(MSG *p,int newsockfd)
{
	char** q=NULL;//查询出来的结果是个字符指针数组,p指向字符指针数组,p指向查询出来的结果
	int row,column;//保存查询出来的行数和列数
	char *errmsg=NULL;//保存的是数据库语句执行失败的错误原因
	char sql[200]={0};
	sprintf(sql,"select * from his_info where username = '%s';",p->username);
	int ret=sqlite3_get_table(db,sql,&q,&row,&column,&errmsg);
	if(ret!=0)
	{
		printf("sqlite3_get_table failed:%s\n",errmsg);
		exit(-1);
	}
#if 0
	p 指向一个字符指针数组
	p[0]  p[1]  p[2] 
	name  age   score
	p[3]  p[4]  p[5]
	xm    24    98
#endif
	//将回传结果发送给服务器
	int i;
	for(i=0;i<(row+1)*column;i+=column)
	{
		//正数右对齐  负数左对齐
		sprintf(p->filedata,"%10s %-30s %-30s",q[i],q[i+1],q[i+2]);
		send(newsockfd,p,sizeof(*p),0);
	}
	//循环结束,已经将查询到的所有内容都发送完毕
	p->type==0;
	//额外多发送一次,用于通知客户端发送完毕
	send(newsockfd,p,sizeof(*p),0);
	//将当前用户的查看历史记录操作写入历史记录表
	char systemtime[50]={0};
	getSystemTime(systemtime);
	sprintf(sql,"insert into his_info values('%s' '%s','HISTORY');",p->username,systemtime);
	mySqlite3Exec(db,sql);
}
//与客户端交互的线程
void *doClientThread(void *p)
{
	MSG s={0};
	int *q=p;
	int newfd=*q;
	while(1)
	{
		int ret=recv(newfd,&s,sizeof(s),0);
		if(ret>0)
		{
			printf("type is %d,username is %s\n",s.type,s.username);
			switch(s.type)//对当前客户端的请求做判断
			{
			case REGISTER://注册请求
				doRegister(&s,newfd);
				break;
			case LOGIN://登录请求
				doLogin(&s,newfd);
				break;
			case LIST://列表请求
				doList(&s,newfd);
				break;
			case GET://下载文件请求
				doGet(&s,newfd);
				break;
			case PUT://上传文件请求
				doPut(&s,newfd);
				break;
			case HISTORY://获取历史记录请求
				doHistory(&s,newfd);
				break;
			}
		}
		else
		{
			printf("当前%d对应的客户端异常断开\n",newfd);
			close(newfd);
			break;
		}
	}
}
int main(int argc, const char *argv[])
{
	//打开数据库文件　创建两个表　用户信息表　历史记录表
	//sqlite3 *db=NULL;
	char sql[200]={0};
	int ret=sqlite3_open("./user.db",&db);
	if(ret!=0)
	{
		printf("sqlite3_open failed");
		exit(-1);
	}
	printf("sqlite3_open sucessful\n");
	sprintf(sql,"create table if not exists user_info(username varchar(20),password varchar(20));");
	mySqlite3Exec(db,sql);
	sprintf(sql,"create table if not exists his_info(username varchar(20),time varchar(20),dosomething varchar(20));");
	mySqlite3Exec(db,sql);


	//搭建多线程并发服务器
	int sockfd=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd==-1)
	{
		perror("socket failed");
		exit(-1);
	}
	//绑定自己的ip地址和端口号
	struct sockaddr_in serveraddr={0};
	serveraddr.sin_family=AF_INET;
	serveraddr.sin_port=htons(5555);
	serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);
	bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
	//设置监听
	listen(sockfd,5);
	//客户端结构体
	struct sockaddr_in clientaddr={0};
	int len=sizeof(clientaddr);
	//创建线程id
	pthread_t id;
	//循环收发数据
	while(1)
	{
		//阻塞等待接收获取新的流式套接字
		int newsockfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
		printf("newsockfd is %d对应的客户端连接了\n",newsockfd);
		//创建一个线程
		pthread_create(&id,NULL,doClientThread,&newsockfd);
	}
	close(sockfd);
	return 0;
}
