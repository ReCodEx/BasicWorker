#ifndef CODEX_WORKER_FILE_SANDBOX_BASE_H
#define CODEX_WORKER_FILE_SANDBOX_BASE_H

#include <memory>
#include <string>
#include <exception>
#include <map>


struct sandbox_limits {
	size_t memory_usage;	//limit memory usage by whole control group (kB)
	float cpu_time;			//limit total run time of whole control group (s)
	float wall_time;		//limit wall time of the program (s)
	float extra_time;		//wait extra time after time limit exceeded (s)
	size_t stack_size;		//limit stack size (kB), 0 = no limit
	size_t files_size;		//limit disk usage (kB), 0 = no limit
	size_t quota_blocks;	//disk quota in blocks
	size_t quota_inodes;	//disk quota in inodes
	std::string stdin;		//redirect stdin from file
	std::string stdout;		//redirect stdout to file
	std::string stderr;		//redirect stderr to file
	std::string chdir;		//change working directory inside sandbox
	size_t processes;		//limit number of processes, 0 = no limit
	bool share_net;			//allow use host network
	std::map<std::string, std::string> environ_vars; //set environment variables
	std::string meta_log;	//save meta-data log to file
};

class sandbox_base {
public:
	virtual ~sandbox_base() {}
	virtual std::string get_dir() const { return sandboxed_dir_; }
	virtual void run(const std::string &binary, const std::string &arguments) = 0;
protected:
	std::string sandboxed_dir_;
};



class sandbox_exception : public std::exception {
public:
	sandbox_exception() : what_("Generic sandbox exception") {}
	sandbox_exception(const std::string &what) : what_(what) {}
	virtual ~sandbox_exception() {}
	virtual const char* what() const noexcept
	{
		return what_.c_str();
	}
protected:
	std::string what_;
};

#endif // CODEX_WORKER_FILE_SANDBOX_BASE_H
