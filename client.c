#include"my.h"
#define REGISTER 1//注册请求
#define LOGIN 2//登录请求
#define LIST 3//获取列表请求
#define GET 4//下载文件请求
#define PUT 5//上传文件请求
#define HISTORY 6//历史记录请求
//通信协议 是一个结构体 传输协议是TCP
typedef struct
{
	int type;//请求的类型
	char username[30];//用户名
	char passwd[30];//用户密码
	char filename[30];//上传或者下载文件的名字
	char filedata[200];//上传或者下载文件的内容
	int size;//用来记录每次实际读取和实际写入的字节数
}MSG;
int sockfd;//为什么定义在全局区？因为每一个请求都需要跟服务器通信 定义在全局区每一个请求函数都可以使用
MSG s = {0};//定义在全局区就是为了让每一个操作使用同一个结构体 为什么每一个操作都要使用同一个结构体？因为用户名在登录之后就不能再改变了 这样传递这个结构体时服务器才知道是谁在执行当前的操作
//客户端提出注册请求
void resisterRequset()
{
	s.type = REGISTER;//装上注册请求
	printf("请您输入您要注册的用户名和密码：\n");
	scanf("%s%s",s.username,s.passwd);
	//将结构体发给服务器处理
	send(sockfd,&s,sizeof(s),0);
	//阻塞等待服务器回传注册结果
	recv(sockfd,&s,sizeof(s),0);
	if(s.type == 0)
	{
		printf("@@@@@@@@@@恭喜您，注册成功！@@@@@@@@@@@\n");
	}
	else if(s.type == -1)
	{
		printf("@@@@@@@@十分抱歉，注册失败！@@@@@@@@@@@\n");
	}
}
//客户端提出获取列表请求
void getListRequest()
{
	//装上请求类型
	s.type = LIST;
	//发送给服务器
	send(sockfd,&s,sizeof(s),0);
	//循环阻塞等待客户端回传读取结果
	printf("当前客户端的个人目录中有以下文件:\n");
	while(1)
	{	
		recv(sockfd,&s,sizeof(s),0);
		if(s.type == 0)//说明读取结束
			break;
		printf("%s\n",s.filename);
	}
}
//客户端提出下载文件功能
void getFileRequest()
{
	char pathname[100] = {0};
	char dirent[50] = {0};
	s.type = GET;
	printf("请输入您需要下载的文件名称:\n");
	scanf("%s",s.filename);
	printf("请输入下载位置:\n");
	scanf("%s",dirent);
	send(sockfd,&s,sizeof(s),0);
	sprintf(pathname,"%s/%s",dirent,s.filename);//将下载到的位置和文件名格式化成一个完整的路径
	int fd = open(pathname,O_WRONLY|O_CREAT|O_TRUNC,0666);
	if(fd == -1)
	{
		perror("open falied");
		exit(-1);
	}
	while(recv(sockfd,&s,sizeof(s),0)>0)
	{
		if(s.type == 0)
		{
			close(fd);
			break;
		}
		write(fd,s.filedata,s.size);
	}
	printf("文件下载完毕!!!\n");
}
//客户端提出上传文件功能
void putFileRequest()
{
	char dirent[50]={0};
	char pathname[100]={0};
	//装上请求类型
	s.type=PUT;
	//输入要上传的文件名称和位置
	printf("请输入您要上传文件的名字:\n");
	scanf("%s",s.filename);
	printf("请您输入这个文件的位置:\n");
	scanf("%s",dirent);
	sprintf(pathname,"%s/%s",dirent,s.filename);
	int fd=open(pathname,O_RDONLY);
	if(fd==-1)
	{
		perror("open failed");
		exit(-1);
	}
	while(s.size = read(fd,s.filedata,sizeof(s.filedata)-1))
	{
		send(sockfd,&s,sizeof(s),0);
	}
	//退出循环说明读取完毕
	s.type=0;
	//额外发送一次告诉服务器已经发送完了
	send(sockfd,&s,sizeof(s),0);
	close(fd);
	printf("文件上传成功!\n");
}
//客户端提出获取历史记录请求
void getHistoryRequest()
{
	//装上请求类型
	s.type=HISTORY;
	//将请求发送给服务器
	send(sockfd,&s,sizeof(s),0);
	//循环等待服务器回传消息
	while(1)
	{
		recv(sockfd,&s,sizeof(s),0);
		if(s.type==0)
		{
			break;
		}
		printf("%s\n",s.filedata);
	}
}
//登录成功后显示功能菜单界面
void showMenu()
{
	while(1)
	{
		printf("///////////////////////////////////\n");
		printf("\n***********************************\n");
		printf("\n      欢 迎 进 入 天 枢 数 据     \n");
		printf("\n//////////////////////////////////\n");
		printf("1.获取列表 2.下载文件 3.上传文件 4.历史记录 5.退出网盘");
		printf("\n***********************************\n");
		printf("请输入您的选择：\n");
		int n;
		scanf("%d",&n);
		switch(n)
		{
		case 1://获取列表请求
			getListRequest();
			break;
		case 2://下载文件请求
			getFileRequest();
			break;
		case 3://上传文件请求
			putFileRequest();
			break;
		case 4://获取历史记录请求
			getHistoryRequest();
			break;
		case 5://退出
			printf("////////感谢您的使用，2s后即将退出！//////\n");
			close(sockfd);
			sleep(2);
			exit(0);//结束客户端
		}
	}
}
//客户端提出登陆请求
void loginRequest()
{
	s.type = LOGIN;//装上请求类型
	printf("请输入您的用户名和密码:\n");
	scanf("%s%s",s.username,s.passwd);
	//将结构体发给服务器处理
	send(sockfd,&s,sizeof(s),0);
	//阻塞等待服务器给我回传登陆结果
	recv(sockfd,&s,sizeof(s),0);
	if(s.type == 0)
	{
		printf("@@@@@@@@@恭喜您，登录成功!@@@@@@@@@@@\n");
		//进入菜单界面
		showMenu();
	}
	else if(s.type == -1)
	{
		printf("@@@@十分抱歉！您输入的用户名有误！@@@@\n");
	}
	else if(s.type == -2)
	{
		printf("@@@@十分抱歉！您输入的密码有误！@@@@\n");
	}
}
int main(int argc, const char *argv[])
{
	if(argc != 3)
	{
		printf("参数传递错误！\n");
		exit(-1);
	}
	//1.创建一个流式套接字
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd == -1)
	{
		perror("socket failed");
		exit(-1);
	}
	//2.去连接服务器
	struct sockaddr_in serveraddr = {0};
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(argv[2]));
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	int ret = connect(sockfd,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
	if(ret == -1)
	{
		perror("connect failed");
		exit(1);
	}
	//3.循环输入你的请求
	while(1)
	{
		int n;
		printf("\n--------------------------------------------\n");
		printf("**        天　　　枢　　　数　　　据        **\n");
		printf("**********************************************\n");
		printf("                                              \n");
		printf("       1.注册     2.登陆     3.退出           \n");
		printf("**********************************************\n");
		printf("                                              \n");
		printf("----------------------------------------------\n");
		printf("请输入您的选择：\n");
		scanf("%d",&n);
		switch(n)//对输入的请求做出判断
		{
		case REGISTER://输入1 代表注册请求
			resisterRequset();
			break;
		case LOGIN://输入2 代表登陆请求
			loginRequest();
			break;
		case 3://想退出
			printf("//////2s后即将退出！欢迎下次使用！//////\n");
			sleep(2);
			close(sockfd);
			exit(0);
		default://输入的是其他值
			printf("//当前输入选项有误 请重新输入！\n//");
			continue;//退出本次循环
		}
	}
	//4.关闭套接字
	close(sockfd);
	return 0;
}
