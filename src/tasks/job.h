#ifndef CODEX_WORKER_JOB_HPP
#define CODEX_WORKER_JOB_HPP

#include <vector>
#include <queue>

#define BOOST_FILESYSTEM_NO_DEPRECATED
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include "spdlog/spdlog.h"
#include "../config/worker_config.h"
#include "task_base.h"
#include "fake_task.h"
#include "external_task.h"
#include "../fileman/file_manager_base.h"
#include "../sandbox/sandbox_base.h"
#include "internal/cp_task.h"
#include "internal/mkdir_task.h"
#include "internal/rename_task.h"
#include "internal/rm_task.h"
#include "internal/archivate_task.h"
#include "internal/extract_task.h"
#include "internal/fetch_task.h"
#include "../helpers/topological_sort.h"
#include "job_metadata.h"
#include "job_tasks.h"


/**
 * Job is unit which is received from broker and should be evaluated.
 * Job is built from configuration in which all information should be provided.
 * Job building results in task tree and task queue in which task should be evaluated.
 */
class job {
public:
	job() = delete;

	/**
	 * Only way to construct a job. All variables are needed to proper function.
	 * @param tasks A collection of tasks in execution order
	 * @param source_path path to source codes of submission
	 * @throws job_exception if there is problem during loading of configuration
	 */
	job(
		std::shared_ptr<job_tasks> tasks,
		fs::path source_path
	);

	~job();

	/**
	 * Runs all task which are sorted in task queue.
	 * Should not throw an exception.
	 */
	void run();

	/**
	 * Get results from all tasks and return them.
	 * @return associative array which indexes are task ids
	 */
	std::map<std::string, std::shared_ptr<task_results>> get_results();

private:
	/**
	 * Cleanup after job evaluation, should be enough to delete all created files
	 */
	void cleanup_job();

	// PRIVATE DATA MEMBERS
	fs::path source_path_;

	/** Tasks in linear ordering prepared for evaluation */
	std::vector<std::shared_ptr<task_base>> task_queue_;
};


#endif //CODEX_WORKER_JOB_HPP
