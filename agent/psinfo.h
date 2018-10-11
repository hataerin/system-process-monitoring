#include <string>
#include <vector>

class sysprocess
{
public:
	sysprocess();
	virtual ~sysprocess();

	int fetch_process_info();
	void clear();
	int getnproc() const;
	const std::string& getname(int n) const;
	const std::string& getcmd(int n) const;
	int getpid(int n) const; 

private:
	class procinfo;
	typedef	std::vector<procinfo*> psinfo_vector;

	psinfo_vector	v;

	class procinfo
	{
	public:
		procinfo();
		procinfo(int pid, int ppid, int pgrp, char *name, char *cmd);
		~procinfo();
		int	getpid() const { return ps_pid; }
		const std::string& getname() const { return ps_name; }
		const std::string& getcmd() const { return ps_cmd; }

	private:
		procinfo(const procinfo&);
		int     ps_pid;
		int     ps_ppid;
		int     ps_pgrp;
		std::string 	ps_name;
		std::string	ps_cmd;
	};
};

