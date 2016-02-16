#include "archivate_task.h"
#include "../../archives/archivator.h"


archivate_task::archivate_task(size_t id,
	std::string task_id,
	size_t priority,
	bool fatal,
	const std::string &cmd,
	const std::vector<std::string> &arguments,
	const std::vector<std::string> &dependencies)
	: task_base(id, task_id, priority, fatal, dependencies, cmd, arguments)
{
	if (arguments_.size() != 2) {
		throw task_exception("Wrong number of arguments. Required: 2, Actual: " + arguments_.size());
	}
}


archivate_task::~archivate_task()
{
}


std::shared_ptr<task_results> archivate_task::run()
{
	try {
		archivator::compress(arguments_[0], arguments_[1]);
		return std::shared_ptr<task_results>(new task_results());
	} catch (archive_exception &e) {
		throw task_exception(std::string("Cannot create archive. Error: ") + e.what());
	}
}
