#ifndef RECODEX_WORKER_JOB_HPP
#define RECODEX_WORKER_JOB_HPP

#include <vector>
#include <queue>
#include <utility>
#include <memory>

#define BOOST_FILESYSTEM_NO_DEPRECATED
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include "spdlog/spdlog.h"
#include "../helpers/logger.h"
#include "../helpers/topological_sort.h"
#include "../config/worker_config.h"
#include "../config/job_metadata.h"
#include "../config/task_metadata.h"
#include "../tasks/task_factory_base.h"
#include "../sandbox/sandbox_base.h"
#include "progress_callback_base.h"


/**
 * Job is unit which is received from broker and should be evaluated.
 * Job is built from configuration in which all information should be provided.
 * Job building results in task tree and task queue in which task should be evaluated.
 * @note During construction job_metadata structure is given. This structure is editable and
 * there is posibility it can be changed by whoever constructed a job class.
 * In actual ReCodEx worker this situation can never happen. But be aware of this and keep it in mind in other coding.
 * Also do not change job_metadata or task_metadata structure between construction of tasks and its execution.
 * If you do it, you should watch your back, devil will be always very close!
 */
class job
{
public:
	job() = delete;

	/**
	 * Only way to construct a job. All variables are needed to proper function.
	 * @param job_meta
	 * @param worker_conf
	 * @param working_directory Directory for temporary saving of files by tasks. Example
	 *							use case is storing isolate's meta log file.
	 * @param source_path path to source codes of submission
	 * @param result_path path to directory containing all results
	 * @param fileman file manager which is provided to tasks
	 * @throws job_exception if there is problem during loading of configuration
	 */
	job(std::shared_ptr<job_metadata> job_meta,
		std::shared_ptr<worker_config> worker_conf,
		fs::path working_directory,
		fs::path source_path,
		fs::path result_path,
		std::shared_ptr<task_factory_base> factory,
		std::shared_ptr<progress_callback_base> progr_callback);

	/**
	 * Job cleanup (if needed) is executed.
	 */
	~job();

	/**
	 * Runs all task which are sorted in task queue and get results from all of them.
	 * Should not throw an exception.
	 * @return Vector with pairs task id - task_results. Values are not @a nullptr.
	 */
	std::vector<std::pair<std::string, std::shared_ptr<task_results>>> run();

	const std::vector<std::shared_ptr<task_base>> &get_task_queue() const;

private:
	/**
	 * Check directories given during construction for existence.
	 */
	void check_job_dirs();
	/**
	 * Init system logger for job. Resulting log will be send with other results to frontend.
	 */
	void init_logger();
	/**
	 * If given progress callback is nullptr, then make it empty callback so we can call it freely.
	 */
	void init_progress_callback();
	/**
	 * Cleanup after job evaluation, should be enough to delete all created files
	 */
	void cleanup_job();
	/**
	 * Build job from @a job_meta_. Should be called in constructor.
	 */
	void build_job();
	/**
	 * Check limits and in case of undefined values set worker defaults.
	 * @param limits limits which will be checked
	 */
	void process_task_limits(std::shared_ptr<sandbox_limits> limits);
	/**
	 * Given unconnected tasks will be connected according to their dependencies.
	 * If they do not have dependency, they will be assigned to given root task.
	 * @param root only task which wont have any parent
	 * @param unconn_tasks given unconnected tasks which will be connected
	 * @param eff_indegree efficient indegree for all tasks
	 */
	void connect_tasks(std::shared_ptr<task_base> root,
		std::map<std::string, std::shared_ptr<task_base>> &unconn_tasks,
		std::map<std::string, size_t> &eff_indegree);

	/**
	 * Prepare variables which can be used in job configuration.
	 */
	void prepare_job_vars();
	/**
	 * In input string tries to find internal job config variable.
	 * If so, then replace variable with actual value and return it.
	 * @param src scanned string for variables
	 * @return new string with all variables replaced with values
	 */
	std::string parse_job_var(const std::string &src);

	// PRIVATE DATA MEMBERS
	/** Information about this job given on construction. */
	std::shared_ptr<job_metadata> job_meta_;
	/** Pointer on default worker config. */
	std::shared_ptr<worker_config> worker_config_;
	/** Directory, where tasks can create their own subfolders and temporary files. */
	fs::path working_directory_;
	/** Directory where source codes needed in job execution are stored. */
	fs::path source_path_;
	/** Directory where results and log of job are stored. */
	fs::path result_path_;
	/** Factory for creating tasks. */
	std::shared_ptr<task_factory_base> factory_;
	/** Progress callback which is called on some important points */
	std::shared_ptr<progress_callback_base> progress_callback_;

	/** Variables which can be used in job configuration */
	std::map<std::string, std::string> job_variables_;

	/** Logical start of every job evaluation */
	std::shared_ptr<task_base> root_task_;
	/** Tasks in linear ordering prepared for evaluation */
	std::vector<std::shared_ptr<task_base>> task_queue_;

	/** Job logger */
	std::shared_ptr<spdlog::logger> logger_;
};


#endif // RECODEX_WORKER_JOB_HPP
