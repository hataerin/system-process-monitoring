#include "logger.h"
#include "psinfo.h"

#include <stdio.h>      
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>


#define HEADER_VALIDATION_CHAR		"FFFF!@#$  "
#define HEADER_VALIDATION_CHAR_LEN	10
#define HEADER_MESSAGE_LEN			10
#define HELLO_MESSAGE				"Hello     "
#define HELLO_MESSAGE_LEN			10
#define	MAX_PROCESS_NAME_LEN		128
#define	MAX_PROCESS_ARG_LEN			128

int gloglevel;

int serverConnection(int* sock, char* serverIP, int serverport);
int getMonitoringProcessList(char (*process_name)[MAX_PROCESS_NAME_LEN], char (*process_arg)[MAX_PROCESS_ARG_LEN]);
int checkMonitoringProcessStatus(char (*process_name)[MAX_PROCESS_NAME_LEN], char (*process_arg)[MAX_PROCESS_ARG_LEN], int* process_cur_status, int* process_status);
int alertMonitoringProcessStatus(int sock, char (*process_name)[MAX_PROCESS_NAME_LEN], char (*process_arg)[MAX_PROCESS_ARG_LEN], int* process_cur_status, int* process_status);
int report(int sock, char* process_name, char* process_arg, int process_cur_status, int process_status);
int sendHello(int sock);
void extract_process_name(char *word, char *line, char stop);
void rtrim(char *r);

#define ISSPACE(p)      ( ' '==(p) || '\t'==(p) || '\r'==(p) || '\n'==(p) )

int main(int argc, char* argv[])
{
	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s ServerIP ServerPort MonitoringIntervalSec\n", argv[0]);
		exit(1);
	}

	char* temploglevel = getenv("LOGLEVEL");
	if (temploglevel == NULL)
		gloglevel = 1;
	else
		gloglevel = atoi(temploglevel);

	if (gloglevel == 0)
		gloglevel = 1;

	int sock = 0;
	char *serverIP = NULL;
	serverIP = argv[1];
	int serverPort = atoi(argv[2]);
	int interval = atoi(argv[3]);

	writelog(LOG_INFO, "%s", "start of agent program");

	int count = 0;
	int sleeptime = 0;
	int cnt = 0;

	int ret = 0;

	ret = serverConnection(&sock, serverIP, serverPort);

	ret = sendHello(sock);

	char process_name[128][MAX_PROCESS_NAME_LEN] = {0};
	char process_arg[128][MAX_PROCESS_ARG_LEN] = {0};

	ret = getMonitoringProcessList(process_name, process_arg);

	int process_cur_status[128] = {0};
	int process_status[128] = {0};

	while(1)
	{
		ret = checkMonitoringProcessStatus(process_name, process_arg, process_cur_status, process_status);

		ret = alertMonitoringProcessStatus(sock, process_name, process_arg, process_cur_status, process_status);

		sleep(interval);
	}

	return 0;
}


int serverConnection(int* sock, char* serverIP, int serverPort)
{
	struct sockaddr_in echoServAddr;
	unsigned short echoServPort;
	int bytesRcvd, totalBytesRcvd;
	char mode[10]={0};

	if ((*sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		writelog(LOG_ERROR, "socket initialize failed %d - %s", errno, strerror(errno));
		return -1;
	}

	memset(&echoServAddr, 0, sizeof(echoServAddr));
	echoServAddr.sin_family      = AF_INET;
	echoServAddr.sin_addr.s_addr = inet_addr(serverIP);
	echoServAddr.sin_port        = htons((unsigned short)serverPort);
	if (connect(*sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
	{
		writelog(LOG_ERROR, "socket connection failed %d - %s", errno, strerror(errno));
		return -1;
	}
	return 0;
}

int sendHello(int sock)
{

	char msg[1024] = {0};
	memset(msg, ' ', 1024);
	memcpy(msg, HEADER_VALIDATION_CHAR, HEADER_VALIDATION_CHAR_LEN);
	memcpy(msg+10, HELLO_MESSAGE, strlen(HELLO_MESSAGE));

	int length = HEADER_VALIDATION_CHAR_LEN + HELLO_MESSAGE_LEN;

	if (send(sock, msg, length, 0) != length)
	{
		writelog(LOG_ERROR, "socket send failed %d - %s", errno, strerror(errno));
		return -1;
	}
	else
	{
		writelog(LOG_INFO, "socket send success");
		return 0;
	}


}

int getMonitoringProcessList(char (*process_name)[MAX_PROCESS_NAME_LEN], char (*process_arg)[MAX_PROCESS_ARG_LEN])
{

	char fileName[128] = {0};
	xmlNodePtr data_node = NULL;
	sprintf(fileName, "./data/systemprocess.xml");

	xmlDocPtr doc = NULL;
	if((doc = xmlParseFile(fileName)) == NULL)
	{
		data_node = xmlNewNode(NULL, (const xmlChar *)"Data");
	}
	else
	{
		xmlNodePtr temp_root = xmlDocGetRootElement(doc);
		data_node = xmlCopyNode(temp_root, 1);
	}

	if(data_node == NULL || data_node->children == NULL)
	{
		writelog(LOG_ERROR, "%s - data node is null", fileName);
		return -1;
	}
	xmlNodePtr pnode = data_node->children;


	int i = 0;
	while(pnode != NULL)
	{
		xmlChar *attr_value = xmlGetProp(pnode, (xmlChar *)"ProcessName");
		if(attr_value != NULL)
		{
			strcpy(process_name[i], (char*)attr_value);
			xmlFree(attr_value);
		}
		else
		{
			pnode = pnode->next;
			continue;
		}
		attr_value = xmlGetProp(pnode, (xmlChar *)"Arguments");
		if(attr_value != NULL)
		{
			strcpy(process_arg[i], (char*)attr_value);
			xmlFree(attr_value);
		}
		writelog(LOG_INFO, "Monitoring System Process[%d]:(ProcessName:%s, Argument:%s)", i+1, process_name[i], process_arg[i]);
		pnode = pnode->next;
		i++;
	}
}

int checkMonitoringProcessStatus(char (*process_name)[MAX_PROCESS_NAME_LEN], char (*process_arg)[MAX_PROCESS_ARG_LEN], int* process_cur_status, int* process_status)
{
	char pid[8] = {0};
	char ppid[8] = {0};
	char proc_name[128] = {0};
	char command[1024] = {0};

	sysprocess v;
	v.fetch_process_info();
	int i = 0;
	while(process_name[i][0] != 0)
	{
		process_cur_status[i] = process_status[i];
		process_status[i] = 0;
		for(int j = 0; j < v.getnproc(); j++)
		{
			sprintf(pid, "%d", v.getpid(j));
			strcpy(proc_name,v.getname(j).data());
			strcpy(command,v.getcmd(j).data());
			rtrim(command);
			if(!command[0]) continue;
			char *arg = command;
			char cmd[1024];
			extract_process_name(cmd, arg, ' ');
			char *cmd2 = strrchr(cmd, '/');
			char *cmd3 = strrchr(cmd, '\\');
			char *filtered_cmd = cmd;
			if(cmd2 > cmd3) filtered_cmd = ++cmd2;
			else if(cmd3 != NULL) filtered_cmd = ++cmd3;
			rtrim(arg);
			if(!strcmp(process_name[i],proc_name))
			{
				if(process_arg[i][0] == 0)
				{
					process_status[i] = 1;
					writelog(LOG_INFO, "Execution Process Found-ProcessName:%s, Arg:%s", process_name[i], process_arg[i]);
					break;
				}
				if(!strcmp(process_arg[i],arg))
				{
					process_status[i] = 1;
					writelog(LOG_INFO, "Execution Process Found-ProcessName:%s, Arg:%s", process_name[i], process_arg[i]);
					break;
				}
			}
		}
		i++;
	}
}

int alertMonitoringProcessStatus(int sock, char (*process_name)[MAX_PROCESS_NAME_LEN], char (*process_arg)[MAX_PROCESS_ARG_LEN], int* process_cur_status, int* process_status)
{
	int i = 0;
	while(process_name[i][0] != 0)
	{
		if(process_cur_status[i] != process_status[i])
		{
			writelog(LOG_INFO, "Server Report Info:ProcessName:%s, Arg:%s, CurStatus:%d, Status:%d", process_name[i], process_arg[i], process_cur_status[i], process_status[i]);
			report(sock, process_name[i], process_arg[i], process_cur_status[i], process_status[i]);
		}
		i++;
	}

}

int report(int sock, char* process_name, char* process_arg, int process_cur_status, int process_status)
{

	char msg[1024] = {0};
	memset(msg, ' ', 1024);
	memcpy(msg+10, process_name, strlen(process_name));
	memcpy(msg+10+128, process_arg, strlen(process_arg));

	char temp[16] = {0};
	if(process_cur_status == 0 && process_status == 1)
		strcpy(temp, "Running");
	else
		strcpy(temp, "Stopped");
	memcpy(msg+10+128+128, temp, strlen(temp));

	int length = 128+128+10;
	char slength[10] = {0};
	sprintf(slength, "%d", length);
	memcpy(msg, slength, 10);


	if (send(sock, msg, length + HEADER_MESSAGE_LEN, 0) != length + HEADER_MESSAGE_LEN)
	{
		writelog(LOG_ERROR, "socket send failed %d - %s", errno, strerror(errno));
		return -1;
	}
	else
	{
		writelog(LOG_INFO, "socket send success");
		return 0;
	}
}


void rtrim(char *r)
{
	if(r == NULL || strlen(r) == 0)
		return;
	char *p = r + strlen(r) - 1;
	while(p >= r && ISSPACE(*p))
		*p-- = '\0';
}


void extract_process_name(char *word, char *line, char stop)
{
	int x, y;
	if(line[0] != '"')
	{
		for (x = 0; ((line[x]) && (line[x] != stop)); x++)
			word[x] = line[x];
	}
	else
	{
		for(x = 1; line[x] && line[x] != '"'; x++)
			word[x] = line[x];
	}

	word[x] = '\0';
	if (line[x])
		++x;
	y = 0;

	while ((line[y++] = line[x++]));
}
