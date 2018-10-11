#if !defined(__INC_SMTP)
#define __INC_SMTP


#define CONNECTION_CHECK                0L
#define HELLO_CHECK                             1L
#define MAIL_CHECK                              2L
#define RCPT_CHECK                              3L
#define DATA_START_CHECK                4L
#define DATA_END_CHECK                  5L
#define QUIT_CHECK                              6L
#define DATA_CHECK                              7L


int Connect(int* smtpsock, const char *IPAddress, int Port);
int Disconnect(int smtpsock);  
int SetFrom(int smtpsock, const char *From);
int SetTo(int smtpsock, const char *To);
int Data(int smtpsock, const char *Subject, const char *Body);
int CheckResponse(int smtpsock, int Type);
char* GetErrorMessage();

#endif
