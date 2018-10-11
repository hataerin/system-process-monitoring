#include "logger.h"
#include "db.h"

#include <stdio.h>      
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <memory.h>
#include <pthread.h>
#include <signal.h>

#define MAXPENDING 1    /* Maximum outstanding connection requests */

#define HEADER_VALIDATION_CHAR		"FFFF!@#$  "
#define HEADER_VALIDATION_CHAR_LEN	10
#define HEADER_MESSAGE_LEN			10
#define HELLO_MESSAGE				"Hello     "
#define HELLO_MESSAGE_LEN			10
#define	MAX_PROCESS_NAME_LEN		128
#define	MAX_PROCESS_ARG_LEN			128

int gloglevel;

void* HandleTCPClient(void* pArg);   /* TCP client handling function */
int recvHello(int sock);
bool isPending(int Socket, int pending, int timeout);


#define ISSPACE(p)      ( ' '==(p) || '\t'==(p) || '\r'==(p) || '\n'==(p) )

void rtrim(char *r)
{
	if(r == NULL || strlen(r) == 0)
		return;
	char *p = r + strlen(r) - 1;
	while(p >= r && ISSPACE(*p))
		*p-- = '\0';
}

int main(int argc, char* argv[])
{
	if (argc < 5)
	{
		printf("Usage : programname port db_name userid userpw\n");
		return 0;
	}

	char* temploglevel = getenv("LOGLEVEL");
	if (temploglevel == NULL)
		gloglevel = 1;
	else
		gloglevel = atoi(temploglevel);

	if (gloglevel == 0)
		gloglevel = 1;

	int ret = 0;
	char logmessage[1024] = { 0, };
	writelog(LOG_INFO, "%s", "start of sockserver program");
	writelog(LOG_INFO, "loglevel:%d", gloglevel);

	char tns[128] = {0,};
	char userid[128] = {0,};
	char userpw[128] = {0,};

	strcpy(tns, argv[2]);
	strcpy(userid, argv[3]);
	strcpy(userpw, argv[4]);

	int servSock;                    
	int clntSock;                   
	struct sockaddr_in servAddr; /* Local address */
	struct sockaddr_in clntAddr; /* Client address */
	unsigned short servPort;     /* Server port */
	int clntLen;            /* Length of client address data structure */
	servPort = atoi(argv[1]);

	// database connection 
	initialize();
	writelog(LOG_INFO, "db connection string - tns:%s, userid:%s, userpw:%s", tns, userid, userpw);
	ret = connect(tns, userid, userpw);
	if(ret)
	{
		writelog(LOG_ERROR, "db connection failed");
		writelog(LOG_ERROR, "program termination");
		return -1;
	}
	else
		writelog(LOG_INFO, "db conection success");

	// server socket open
	if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{	
		writelog(LOG_ERROR, "socket() failed, %d - %s", errno, strerror(errno));
		disconnect();
		uninitialize();
		return -1;
	}
	memset(&servAddr, 0, sizeof(servAddr));   /* Zero out structure */
	servAddr.sin_family = AF_INET;                /*  address family */
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* incoming interface */
	servAddr.sin_port = htons(servPort);      /* Local port */

	int optval = 1;
	setsockopt (servSock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

	if (bind(servSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
	{
		writelog(LOG_ERROR, "bind() failed, %d - %s", errno, strerror(errno));
		disconnect();
		uninitialize();
		return -1;
	}
	if (listen(servSock, MAXPENDING) < 0)
	{
		writelog(LOG_ERROR, "listen() failed, %d - %s", errno, strerror(errno));
		disconnect();
		uninitialize();
		return -1;
	}

	pthread_t th;

	while(1) /* Run forever */
	{
		clntLen = sizeof(clntAddr);
		clntSock = accept(servSock, (struct sockaddr *)&clntAddr, (socklen_t*)&clntLen);
		if(clntSock < 0)
		{
			writelog(LOG_ERROR, "accept() failed, %d - %s", errno, strerror(errno));
			disconnect();
			uninitialize();
			return -1;
		}
		writelog(LOG_INFO, "Handling client IP:%s, Port:%d", inet_ntoa(clntAddr.sin_addr), clntAddr.sin_port);

		//HandleTCPClient((void*)&clntSock);
		pthread_create(&th, NULL, HandleTCPClient, (void*)&clntSock);
		
	}
	return 0;
}

void* HandleTCPClient(void* pArg)
{
	int clntSocket = *(int*)pArg;
	writelog(LOG_INFO, "start of HandleTCPClient(thread). clntSock:%d", clntSocket);

	int ret = 0;
	ret = recvHello(clntSocket);
	if(ret)
	{
		writelog(LOG_INFO, "detected bad connection. close connection");
		return NULL;
	}

	while(1)
	{
		writelog(LOG_INFO, "Service Ready");

		int ret = 0;
		int length = 10;
		char* buffer = NULL;        /* Buffer for echo string */
		buffer = (char*)malloc(length+1);
		memset(buffer, 0, length+1);
		int recvMsgSize = 0;                    /* Size of received message */
		int getSize = 0;
		char* cur = buffer;
		while (getSize < length)
		{
			ret = isPending(clntSocket, 1, 30000);
			if(ret == 0)
			{
				writelog(LOG_INFO, "connection recv select timeout (30Sec)");
				continue;
			}

			if ((recvMsgSize = recv(clntSocket, cur, length-getSize, 0)) <= 0)
			{
				if(recvMsgSize == 0)
					writelog(LOG_INFO, "client connection disconnected");
				else
					writelog(LOG_ERROR, "recv() failed, %d - %s", errno, strerror(errno));
				close(clntSocket);
				return NULL;
			}
			writelog(LOG_INFO, "recv message:%s", cur);
			cur = cur + recvMsgSize;
			getSize = getSize + recvMsgSize;
		}

		writelog(LOG_INFO, "socket recv success. recvlen : %d", getSize);
		writelog(LOG_INFO, "socket recv message :%s", buffer);

		length = atoi(buffer);
		char* msg = NULL;
		msg = (char*)malloc(length+1);
		memset(msg, 0, length+1);
		cur = msg;

		free(buffer);
		getSize = 0;
		while (getSize < length)
		{
			ret = isPending(clntSocket, 1, 30000);
			if(ret == 0)
			{
				writelog(LOG_ERROR, "connection recv select failed - timeout (30Sec)");
				free(msg);
				close(clntSocket);
				return NULL;
			}
			if ((recvMsgSize = recv(clntSocket, cur, length-getSize, 0)) <= 0)
			{
				if(recvMsgSize == 0)
					writelog(LOG_INFO, "client connection disconnected");
				else
					writelog(LOG_ERROR, "recv() failed, %d - %s", errno, strerror(errno));
				close(clntSocket);
				return NULL;
			}
			writelog(LOG_INFO, "recv message:%s", cur);
			cur = cur + recvMsgSize;
			getSize = getSize + recvMsgSize;
		} // end of while

		writelog(LOG_INFO, "total recv msglen:%d", getSize);
		writelog(LOG_INFO, "total recv msg:[%s]", msg);

		char proc_name[128+1] = {0};
		char proc_arg[128+1] = {0};
		char proc_status[10+1] = {0};
		memcpy(proc_name, msg, 128);
		memcpy(proc_arg, msg+128, 128);
		memcpy(proc_status, msg+128+128, 10);

		free(msg);

		rtrim(proc_name);
		rtrim(proc_arg);
		rtrim(proc_status);

		char tmp[1024] = {0};
		sprintf(tmp, "processName:%s, processArg:%s, Status:%s\n", proc_name, proc_arg, proc_status);

		// alert내용을 파일로 출력
		FILE* fp = NULL;
		fp = fopen("alert.txt", "a");
		if(fp == NULL)
			writelog(LOG_ERROR, "file open error");
		fwrite(tmp, 1, strlen(tmp), fp);
		fclose(fp);
		
		// alert내용을 db에 입력
		ret = execute_insert(tmp);
		if(ret == 0)
			writelog(LOG_INFO, "db execute_insert success. msg:%s", tmp);
		else
			writelog(LOG_ERROR, "db execute_insert failed");

		commit();
	}

	writelog(LOG_INFO, "%s", "end of HandleTCPClient");
	close(clntSocket);

	return NULL;
}

int recvHello(int sock)
{
	int ret = 0;
	int length = 20;
	char* buffer = NULL;        /* Buffer for echo string */
	buffer = (char*)malloc(length+1);
	memset(buffer, 0, length+1);
	int recvMsgSize = 0;                    /* Size of received message */
	int getSize = 0;
	char* cur = buffer;
	while (getSize < length)
	{
		ret = isPending(sock, 1, 30000);
		if(ret == 0)
		{
			writelog(LOG_INFO, "[recvHello] connection recv select timeout (30Sec).");
			return -1;
		}

		if ((recvMsgSize = recv(sock, cur, length-getSize, 0)) <= 0)
		{
			if(recvMsgSize == 0)
				writelog(LOG_INFO, "client connection disconnected");
			else
				writelog(LOG_ERROR, "recv() failed, %d - %s", errno, strerror(errno));
			close(sock);
			return -1;
		}
		//writelog(LOG_INFO, "recv message:%s", cur);
		cur = cur + recvMsgSize;
		getSize = getSize + recvMsgSize;
	}

	writelog(LOG_INFO, "socket recv success. recvlen : %d", getSize);
	writelog(LOG_INFO, "socket recv message :%s", buffer);

	if(!strncmp(buffer, HEADER_VALIDATION_CHAR, 10) && !strncmp(buffer+10, HELLO_MESSAGE, 10))
	{
		writelog(LOG_INFO, "recv Hello Message validation check success");
		return 0;
	}
	else
	{
		writelog(LOG_INFO, "recv Hello Message validation check failed");
		return -1;
	}
}

bool isPending(int Socket, int pending, int timeout)
{
	int status;
	struct timeval tv;
	struct timeval *tvp = &tv;
	fd_set grp;

	if (timeout == 0)
		tvp = NULL;
	else
	{
		tv.tv_usec = (timeout % 1000) * 1000;
		tv.tv_sec = timeout / 1000;
	}

	FD_ZERO(&grp);
	FD_SET((unsigned int)Socket, &grp);
	int number = 0;
	number = Socket + 1;
	switch (pending)
	{
	case 1: // pendingInput
		status = select(number, &grp, NULL, NULL, tvp);
		break;
	case 2: // pendingOutput
		status = select(number, NULL, &grp, NULL, tvp);
		break;
	case 3: // pendingError
		status = select(number, NULL, NULL, &grp, tvp);
		break;
	}
	if (status < 1) {
		return false;
	}
	if (FD_ISSET(Socket, &grp))
		return true;
	return false;

}

