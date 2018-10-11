#include "logger.h"
#include "smtp.h"

#include <stdio.h>      
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <memory.h>
#include <oci.h>

#ifndef WIN32
#include <unistd.h>
#endif

OCIEnv *envhp;
OCIError *errhp;
OCISession *authp;
OCIServer *srvhp;
OCISvcCtx *svchp;

int initialize();
int connect(const char* cname, const char* uname, const char* passwd);
int commit();
int rollback();
int disconnect();
int uninitialize();
int execute_insert(char* msg);
int execute_select(char* msg);
int execute_update_post();
int execute_update_pre();

int gloglevel;

int main(int argc, char* argv[])
{
        if (argc < 8)
        {
                printf("Usage: %s smtp_address from_address send_address interval db_name userid passwd\n", argv[0]);
                exit(1);
        }
	int ret = 0;

	char smtp[128] = {0};
	char fromaddress[128] = {0};
	char sendaddress[128] = {0};
	int interval = 0;
	char dbname[128] = {0};
	char dbuserid[128] = {0};
	char dbpasswd[128] = {0};

	strcpy(smtp, argv[1]);
	strcpy(fromaddress, argv[2]);
	strcpy(sendaddress, argv[3]);
	interval = atoi(argv[4]);
	strcpy(dbname, argv[5]);
	strcpy(dbuserid, argv[6]);
	strcpy(dbpasswd, argv[7]);

	gloglevel = 1;
	writelog(LOG_INFO, "%s", "start of edaemon program");

	initialize();
	connect(dbname, dbuserid, dbpasswd);

	char msg[1024+1] = {0};

	while(1)
	{
		ret = execute_update_pre();
		ret = execute_select(msg);
		if(ret)
		{
			if(ret == OCI_NO_DATA)
			{
				sleep(interval);
				continue;
			}
			break;
		}
		ret = execute_update_post();
		ret = commit();

		int sock = 0;
		ret = Connect(&sock, smtp, 25);
		if(ret)
		{
			writelog(LOG_ERROR, "smtp connect failed -%s", GetErrorMessage());
			break;
		}
		ret = SetFrom(sock, fromaddress);
		if(ret)
		{
			writelog(LOG_ERROR, "smtp SetFrom failed -%s", GetErrorMessage());
			break;
		}
		ret = SetTo(sock, sendaddress);
		if(ret)
		{
			writelog(LOG_ERROR, "smtp SetTo failed -%s", GetErrorMessage());
			break;
		}
		ret = Data(sock, "Process Status Alert Notification", msg);
		if(ret)
		{
			writelog(LOG_ERROR, "smtp Data send failed -%s", GetErrorMessage());
			break;
		}
		Disconnect(sock);
		writelog(LOG_INFO, "smtp Data send success");
		
		
	}

	disconnect();
	uninitialize();
	writelog(LOG_INFO, "end of edaemon program");

	return 0;
}

int execute_select(char* msg)
{
	int ret = 0;
	sb4 errcode;
	text errbuf[1024];
	OCIStmt *sms_select;
	text* sms_select_stmt = (text*)"select msg from sms where flag = 'P' and rownum = 1";



	ret = OCIHandleAlloc((dvoid *) envhp, (dvoid **) &sms_select, 
						 OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIHandleAlloc() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	ret = OCIStmtPrepare(sms_select, errhp, (text*)sms_select_stmt, 
						 (ub4) strlen((char *) sms_select_stmt), 
						 (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIStmtPrepare() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}


	
	OCIDefine *def_r1 = (OCIDefine*) NULL;
	//OCIDefine *def_r2 = (OCIDefine*) NULL;

    ret = OCIDefineByPos(sms_select, &def_r1, errhp, 1,
            (dvoid *) msg,
            1024+1,
            SQLT_STR, (dvoid *)0, (ub2 *)0,
            (ub2 *) 0, OCI_DEFAULT);;
/*
    ret = OCIDefineByPos(sms_select, &def_r2, errhp, 2,
            (dvoid *) sname,
            (sword) sizeof(sname),
            SQLT_STR, (dvoid *)0, (ub2 *)0,
            (ub2 *) 0, OCI_DEFAULT);;
*/

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIDefineByPos() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}


	ret = OCIStmtExecute(svchp, sms_select, errhp, (ub4) 1, (ub4) 0, 
						 (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIStmtExecute() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);
		writelog(LOG_ERROR,"OCIStmtExecute failed. reason : %s", errbuf);
		return ret;
	}

	if(ret == OCI_NO_DATA)
	{
		writelog(LOG_INFO,"OCIStmtExecute no data..");
		return ret;
	}
	else
	{
		int i = 1;
		while(ret == OCI_SUCCESS)
		{
			writelog(LOG_INFO,"selected data - row%d, - msg:%s", i, msg);
			ret = OCIStmtFetch(sms_select, errhp, 1,
						OCI_FETCH_NEXT, OCI_DEFAULT);
			i++;
		}
	}

	return 0;
}

int execute_update_pre()
{
        int ret = 0;
        sb4 errcode;
        text errbuf[1024];

        OCIStmt *sms_update_pre;
        text* sms_update_pre_stmt = (text*)"update sms set flag = 'P' where flag = 'N' and rownum = 1";
        ret = OCIHandleAlloc((dvoid *) envhp, (dvoid **) &sms_update_pre,
                                                 OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0);
        if(ret < 0) {
                errcode = 0;
                memset(errbuf, 0, sizeof(errbuf));
                writelog(LOG_ERROR,"[ERR] OCIHandleAlloc() failed");
                OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
                                errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
                writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);
                return ret;
        }

        ret = OCIStmtPrepare(sms_update_pre, errhp, (text*)sms_update_pre_stmt,
                                                 (ub4) strlen((char *) sms_update_pre_stmt),
                                                 (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT);
        if(ret < 0) {

                errcode = 0;
                memset(errbuf, 0, sizeof(errbuf));
                writelog(LOG_ERROR,"[ERR] OCIStmtPrepare() failed");
                OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
                                errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
                writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);
                return ret;
        }

        if(ret < 0) {

                errcode = 0;
                memset(errbuf, 0, sizeof(errbuf));
                writelog(LOG_ERROR,"[ERR] OCIDefineByPos() failed");
                OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
                                errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
                writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);
                return ret;
        }
        ret = OCIStmtExecute(svchp, sms_update_pre, errhp, (ub4) 1, (ub4) 0,
                                                 (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT);
        if(ret < 0) {

                errcode = 0;
                memset(errbuf, 0, sizeof(errbuf));

                writelog(LOG_ERROR,"[ERR] OCIStmtExecute() failed");
                OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
                                errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
                writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);
                writelog(LOG_ERROR,"OCIStmtExecute failed. reason : %s", errbuf);
                return ret;
        }
        return 0;
}

int execute_update_post()
{
        int ret = 0;
        sb4 errcode;
        text errbuf[1024];

        OCIStmt *sms_update_post;
        text* sms_update_post_stmt = (text*)"update sms set flag = 'Y' where flag = 'P'";
        ret = OCIHandleAlloc((dvoid *) envhp, (dvoid **) &sms_update_post,
                                                 OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0);
        if(ret < 0) {
                errcode = 0;
                memset(errbuf, 0, sizeof(errbuf));
                writelog(LOG_ERROR,"[ERR] OCIHandleAlloc() failed");
                OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
                                errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
                writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);
                return ret;
        }

        ret = OCIStmtPrepare(sms_update_post, errhp, (text*)sms_update_post_stmt,
                                                 (ub4) strlen((char *) sms_update_post_stmt),
                                                 (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT);
        if(ret < 0) {

                errcode = 0;
                memset(errbuf, 0, sizeof(errbuf));
                writelog(LOG_ERROR,"[ERR] OCIStmtPrepare() failed");
                OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
                                errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
                writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);
                return ret;
        }

        if(ret < 0) {

                errcode = 0;
                memset(errbuf, 0, sizeof(errbuf));
                writelog(LOG_ERROR,"[ERR] OCIDefineByPos() failed");
                OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
                                errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
                writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);
                return ret;
        }
        ret = OCIStmtExecute(svchp, sms_update_post, errhp, (ub4) 1, (ub4) 0,
                                                 (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT);
        if(ret < 0) {

                errcode = 0;
                memset(errbuf, 0, sizeof(errbuf));

                writelog(LOG_ERROR,"[ERR] OCIStmtExecute() failed");
                OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
                                errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
                writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);
                writelog(LOG_ERROR,"OCIStmtExecute failed. reason : %s", errbuf);
                return ret;
        }
        return 0;
}


int initialize()
{
	int	ret;
	sb4 errcode;
	text errbuf[1024];

	ret = OCIInitialize((ub4) OCI_DEFAULT, (dvoid *)0, 
		                (dvoid * (*)(dvoid *, size_t)) 0,
		                (dvoid * (*)(dvoid *, dvoid *, size_t))0,
		                (void (*)(dvoid *, dvoid *)) 0 );

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIInitialize() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}


	ret = OCIEnvInit( (OCIEnv **) &envhp, OCI_DEFAULT, (size_t) 0,
		(dvoid **) 0 );

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIEnvInit() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}



	ret = OCIHandleAlloc((dvoid *)envhp, (dvoid **)&errhp, 
						 OCI_HTYPE_ERROR, (size_t) 0, (dvoid **) 0);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIHandleAlloc(err) failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	/* server contexts */

	ret = OCIHandleAlloc((dvoid *)envhp, (dvoid **)&srvhp, 
						 OCI_HTYPE_SERVER, (size_t) 0, (dvoid **) 0);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIHandleAlloc(srv) failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	ret = OCIHandleAlloc((dvoid *)envhp, (dvoid **)&svchp, 
						 OCI_HTYPE_SVCCTX, (size_t) 0, (dvoid **) 0);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIHandleAlloc(svc) failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}



	return 0;
}

int connect(const char* cname, const char* uname, const char* passwd)
{
	int	ret;
	sb4 errcode;
	text errbuf[1024];


					    // dbconn   length
	ret = OCIServerAttach( srvhp, errhp, (text *)cname, (sb4)strlen(cname), 0);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIServerAttach() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	ret = OCIAttrSet( (dvoid *) svchp, OCI_HTYPE_SVCCTX, (dvoid *)srvhp,
		(ub4) 0, OCI_ATTR_SERVER, (OCIError *) errhp);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIAttrSet() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	ret = OCIHandleAlloc((dvoid *) envhp, (dvoid **)&authp,
		(ub4) OCI_HTYPE_SESSION, (size_t) 0, (dvoid **) 0);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIHandleAlloc() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	ret = OCIAttrSet((dvoid *) authp, (ub4) OCI_HTYPE_SESSION,
		(dvoid *) uname, (ub4) strlen((char *)uname),
		(ub4) OCI_ATTR_USERNAME, errhp);

	ret = OCIAttrSet((dvoid *) authp, (ub4) OCI_HTYPE_SESSION,
		(dvoid *) passwd, (ub4) strlen((char *)passwd),
		(ub4) OCI_ATTR_PASSWORD, errhp);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIAttrSet() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	// c.f OCISessionGet
	ret = OCISessionBegin ( svchp,  errhp, authp, OCI_CRED_RDBMS,
		(ub4) OCI_DEFAULT);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCISessionBegin() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	ret = OCIAttrSet((dvoid *) svchp, (ub4) OCI_HTYPE_SVCCTX,
		(dvoid *) authp, (ub4) 0,
		(ub4) OCI_ATTR_SESSION, errhp);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIAttrSet() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}



	return 0;
}

int commit()
{
	sword ret;
	sb4 errcode;
	text errbuf[1024];

	ret = OCITransCommit(svchp, errhp, (ub4) 0);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCITransCommit() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}
	return 0;
}

int rollback()
{
	sword ret;
	sb4 errcode;
	text errbuf[1024];

	ret = OCITransRollback(svchp, errhp, (ub4) OCI_DEFAULT);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCITransRollback() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}
	return 0;
}

int disconnect()
{
	sword ret;
	sb4 errcode;
	text errbuf[1024];

	// c.f OCISessionRelease
	ret = OCISessionEnd(svchp, errhp, authp, OCI_DEFAULT);
	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCISessionEnd() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	ret = OCIHandleFree(authp, (ub4) OCI_HTYPE_SESSION);
	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIHandleFree() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	ret = OCIServerDetach(srvhp, errhp, OCI_DEFAULT);
	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIServerDetach() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}
	return 0;
}

int uninitialize()
{
	sword ret;
	sb4 errcode;
	text errbuf[1024];

	ret = OCIHandleFree(svchp, OCI_HTYPE_SVCCTX);

	ret = OCIHandleFree(srvhp, OCI_HTYPE_SERVER);

	ret = OCIHandleFree(errhp, OCI_HTYPE_ERROR);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIHandleFree() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}
	return 0;
}



int execute_insert(char* msg)
{
	int ret = 0;
	sb4 errcode;
	text errbuf[1024];

	char empno[1024] = {0,};
	char ename[1024] = {0,};
	char sal[1024] = {0,};

	int iempno = 0;
	int isal = 0;

	OCIStmt *sms_insert;

	OCIBind	*a1 = (OCIBind *)NULL;

	text* sms_insert_stmt = (text*)"insert into sms(sms_id, send_phone, recv_phone, msg, reg_date, flag) "
		"values (seqno_sms.nextval, '010-0000-0000', '010-1111-2222', :msg, sysdate, 'N')";



	ret = OCIHandleAlloc((dvoid *) envhp, (dvoid **) &sms_insert, 
						 OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIHandleAlloc() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}

	ret = OCIStmtPrepare(sms_insert, errhp, (text*)sms_insert_stmt, 
						 (ub4) strlen((char *) sms_insert_stmt), 
						 (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIStmtPrepare() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);

		return ret;
	}


	
	ret = OCIBindByName(sms_insert, &a1, errhp, (text*)":msg", -1, 
						(dvoid *) msg, strlen(msg)+1, 
						SQLT_STR, (dvoid *) NULL, (ub2 *) 0, (ub2 *) 0, (ub4) 0, 
						(ub4 *) 0, OCI_DEFAULT);


	ret = OCIStmtExecute(svchp, sms_insert, errhp, (ub4) 1, (ub4) 0, 
						 (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT);

	if(ret < 0) {

		errcode = 0;
		memset(errbuf, 0, sizeof(errbuf));

		writelog(LOG_ERROR,"[ERR] OCIStmtExecute() failed");
		OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
				errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		writelog(LOG_ERROR,"[ERR CONTENT : %s", errbuf);
		writelog(LOG_ERROR,"OCIStmtExecute failed. reason : %s", errbuf);


		return ret;
	}

	return 0;
}

