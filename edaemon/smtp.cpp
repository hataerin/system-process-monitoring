#include "smtp.h"
#include "logger.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

char            m_ErrorMessage[256];
char            m_From[256];    
char            m_To[256]; 
int         m_smtpsocket;

int Connect(int* smtpsock, const char *IPAddress, int Port)
{

	int sock;
        struct sockaddr_in echoServAddr;
        unsigned short echoServPort;
        char *servIP;
        int bytesRcvd, totalBytesRcvd;
        char mode[10]={0};
        int count = 0;
        int sleeptime = 0;
        int cnt = 0;

        if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        {
                writelog(LOG_ERROR, "socket initialize failed %d - %s", errno, strerror(errno));
                return -1;
        }

        memset(&echoServAddr, 0, sizeof(echoServAddr));
        echoServAddr.sin_family      = AF_INET;
        echoServAddr.sin_addr.s_addr = inet_addr(IPAddress);
        echoServAddr.sin_port        = htons(Port);
        if (connect(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        {
                writelog(LOG_ERROR, "socket connection failed %d - %s", errno, strerror(errno));
                return -1;
        }
        else
        {
                if(CheckResponse(sock, CONNECTION_CHECK) == -1) return -1;

                // HELO를 전송한다.
                char buf[512];
                sprintf(buf, "HELO %s\r\n", IPAddress);
		writelog(LOG_INFO, "send:%s", buf);
		if (send(sock, buf, strlen(buf), 0) != strlen(buf))
		{
			strcpy(m_ErrorMessage, "send message failed");
			return -1;
		}

                if(CheckResponse(sock, HELLO_CHECK) == -1) 
			return -1;
		
        }

	*smtpsock = sock;
	return 0;
}

int Disconnect(int smtpsock)
{
        char buf[256];
        sprintf(buf, "QUIT \r\n");
	writelog(LOG_INFO, "send:%s", buf);
	send(smtpsock, buf, strlen(buf), 0);
        if(CheckResponse(smtpsock, QUIT_CHECK) == -1) return -1;
        else
        {
                close(smtpsock);
                return 0;
        }
}

int SetFrom(int smtpsock, const char *From)
{
        char buf[256];

        strcpy(m_From, From);

        sprintf(buf, "MAIL FROM: <%s>\r\n", From);
	writelog(LOG_INFO, "send:%s", buf);
	if (send(smtpsock, buf, strlen(buf), 0) != strlen(buf))
	{
		strcpy(m_ErrorMessage, "send message failed");
		return -1;
	}
        if (CheckResponse(smtpsock, MAIL_CHECK) == -1) return -1;
        else return 0;
}

int SetTo(int smtpsock, const char *To)
{
        char buf[256];

        strcpy(m_To, To);

        sprintf(buf, "RCPT TO: <%s>\r\n", To);
	writelog(LOG_INFO, "send:%s", buf);
	if (send(smtpsock, buf, strlen(buf), 0) != strlen(buf))
	{
		strcpy(m_ErrorMessage, "send message failed");
		return -1;
	}
        if (CheckResponse(smtpsock, RCPT_CHECK) == -1) return -1;
        else return 0;
}

int Data(int smtpsock, const char *Subject, const char *Body)
{
        char buf[256];
	int ret = 0;

        sprintf(buf, "DATA\r\n");
	writelog(LOG_INFO, "send:%s", buf);
	ret = send(smtpsock, buf, strlen(buf), 0);
	if(ret != strlen(buf))
	{
		writelog(LOG_ERROR, "Data-send failed. ret:%d, %s", strerror(errno));
		return -1;
	}
	writelog(LOG_INFO, "Data-send size:%d", ret);
        if (CheckResponse(smtpsock, DATA_CHECK) == -1) 
	{
		writelog(LOG_ERROR, "Data-CheckResponse error");
		return -1;
	}
        else
        {
                sprintf (buf, "From:%s\r\n", m_From);
		writelog(LOG_INFO, "send:%s", buf);
		send(smtpsock, buf, strlen(buf), 0);

                sprintf (buf, "To:%s\r\n", m_To);
		writelog(LOG_INFO, "send:%s", buf);
		send(smtpsock, buf, strlen(buf), 0);

                sprintf (buf, "SUBJECT:%s\r\n", Subject);
		writelog(LOG_INFO, "send:%s", buf);
		send(smtpsock, buf, strlen(buf), 0);

                sprintf (buf, "\r\n%s\r\n", Body);
		writelog(LOG_INFO, "send:%s", buf);
		send(smtpsock, buf, strlen(buf), 0);

                sprintf (buf, ".\r\n");
		writelog(LOG_INFO, "send:%s", buf);
		send(smtpsock, buf, strlen(buf), 0);

                return 0;
        }
}

int CheckResponse(int smtpsock, int Type)
{
        char buf[1024];
        char temp[4];
	int ret = 0;

        memset(buf, 0, sizeof(buf));

        // Receive the data from Server
        ret = recv(smtpsock, buf, 1024, 0);
	if(ret <= 0)
	{
		writelog(LOG_ERROR, "recv failed. ret:%d, %s",ret, strerror(errno));
	}
	writelog(LOG_INFO, "recv. ret:%d, [%s]", ret, buf);
	
	
        strncpy(temp, buf, 3);
        temp[3] = 0;
        int temp2 = atol(temp);
        switch (Type)
        {
        case CONNECTION_CHECK:
                if (temp2 == 220)
                {
                        strcpy(m_ErrorMessage, "Service is ready");
                        return 0;
                }
                break;
        case HELLO_CHECK:
                if (temp2 == 250)
                {
                        strcpy(m_ErrorMessage, "Requested mail action okay, completed");
                        return 0;
                }
                break;
        case MAIL_CHECK:
                if (temp2 == 250)
                {
                        strcpy(m_ErrorMessage, "Requested mail action okay, completed");
                        return 0;
                }
                break;
        case RCPT_CHECK:
                if (temp2 == 250)
                {
                        strcpy(m_ErrorMessage, "Requested mail action okay, completed");
                        return 0;
                }
                break;
        case DATA_START_CHECK:
                if (temp2 == 354)
                {
                        strcpy(m_ErrorMessage, "Start mail input; end with <CRLF>.<CRLF>");
                        return 0;
                }
                break;
        case DATA_END_CHECK:
                if (temp2 == 250)
                {
                        strcpy(m_ErrorMessage, "Requested mail action okay, completed");
                        return 0;
                }
                break;
        case QUIT_CHECK:
                if (temp2 == 221)
                {
                        strcpy(m_ErrorMessage, "Service closing transmission channel");
                        return 0;
                }
                break;
        case DATA_CHECK:
                if (temp2 == 354)
                {
                        strcpy(m_ErrorMessage, "Start mail input; end with <CRLF>.<CRLF>");
                        return 0;
                }
                break;
        }

        strcpy(m_ErrorMessage, "Error...");

        return -1;
}

char* GetErrorMessage()
{
        return m_ErrorMessage;
}
