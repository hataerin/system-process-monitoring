
#include "psinfo.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

sysprocess::procinfo::procinfo() : ps_cmd()
{
	ps_pid = ps_ppid = ps_pgrp = 0;
}

sysprocess::procinfo::procinfo(int pid, int ppid, int pgrp, char *name, char *cmd)
	: ps_name(name), ps_cmd(cmd)
{
	ps_pid  = pid;
	ps_ppid = ppid;
	ps_pgrp = pgrp;

}
sysprocess::procinfo::~procinfo()
{
}

sysprocess::sysprocess()
{
}

sysprocess::~sysprocess()
{
	clear();
}

int sysprocess::getnproc() const
{
	return v.size();
}

const std::string& sysprocess::getname(int n) const
{
	return v[n]->getname();
}

const std::string& sysprocess::getcmd(int n) const
{
	return v[n]->getcmd();
}

int sysprocess::getpid(int n) const
{
	return v[n]->getpid();
}

void sysprocess::clear()
{
	int i = v.max_size();
	psinfo_vector::iterator iter;
	for(iter=v.begin(); iter != v.end(); iter++)
	{
		delete (*iter);
		(*iter) = NULL;
	}
	v.clear();
}


int sysprocess::fetch_process_info()
{
	clear();


	system("ps -ef > ./data/ps.tmp");

	FILE *fp = fopen("./data/ps.tmp", "rt");
	if(fp == NULL) return -1;

	char user[48];
	char pid[8] = {0};
	char ppid[8] = {0};
	char path[128] = {0};
	char command[1024] = {0};
	char arguments[4096] = {0};

	char line[4097], ln[4097];
	while(fgets(line, 4096, fp) != NULL)
	{
		line[4096] = '\0';
		strcpy(ln, line);
		strcpy(command, "");
		strcpy(arguments, "");

		// -- USER
		char *tmp = strtok(ln, " \t\n");
		if(tmp != NULL)	strcpy(user, tmp);
		
		// -- PID
		tmp = strtok(NULL, " \t\n");
		if(tmp != NULL) strcpy(pid,tmp);
		
		// -- PPID
		tmp = strtok(NULL, " \t\n");
		if(tmp != NULL) strcpy(ppid,tmp);

                // -- C
                tmp = strtok(NULL, " \t\n");

                // -- STIME
                tmp = strtok(NULL, " \t\n");

                // -- TTY
                tmp = strtok(NULL, " \t\n");

		// -- TIME 
                tmp = strtok(NULL, " \t\n");

			tmp = strtok(NULL, " \t\n");

			if(tmp != NULL)
			{
				int l = 0;
				for(l = strlen(tmp); l > 0; l--) if(tmp[l] == '/') break;
				if(l > 0) 
				{
					strcpy(command,tmp + l + 1);
					tmp[l] = '\0';
					strcpy(path,tmp);
				}
				else 
				{
					strcpy(command, tmp);
					strcpy(path,"");
				}
			}
			
			// -- ARGUMENTS
			tmp = strtok(NULL, "\n");
			if(tmp != NULL)
			{
				strcpy(arguments,tmp);
				//arguments.Trim();
			}

			char cmd[4096];
			if(!arguments[0]) strcpy(cmd, command);
			else sprintf(cmd, "%s %s", (char *)command, (char *)arguments);

			v.insert(
				v.end(),
				new procinfo(atol((char *)pid),	atol((char *)ppid), 0, (char *)command, cmd));
	}

	fclose(fp);

	return 0;
}

