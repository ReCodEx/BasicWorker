#include "external_task.h"
#include "../sandbox/isolate_sandbox.h"
#include "../helpers/string_utils.h"
#include "../helpers/filesystem.h"
#include <fstream>
#include <algorithm>
#define BOOST_FILESYSTEM_NO_DEPRECATED
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

external_task::external_task(const create_params &data)
	: task_base(data.id, data.task_meta), worker_config_(data.worker_conf), sandbox_(nullptr),
	  sandbox_config_(data.task_meta->sandbox), limits_(data.limits), logger_(data.logger), temp_dir_(data.temp_dir),
	  evaluation_dir_(data.source_path), working_dir_(data.working_path)
{
	if (worker_config_ == nullptr) {
		throw task_exception("No worker configuration provided.");
	}

	if (limits_ == nullptr) {
		throw task_exception("No limits provided.");
	}

	if (sandbox_config_ == nullptr) {
		throw task_exception("No sandbox configuration provided.");
	}

	if (!sandbox_config_->working_directory.empty()) {
		if (!helpers::check_relative(sandbox_config_->working_directory)) {
			throw task_exception("Given working directory in sandbox config is not relative");
		}

		evaluation_dir_ = fs::path(data.source_path) / sandbox_config_->working_directory;
	}

	sandbox_check();
}

external_task::~external_task()
{
}

void external_task::sandbox_check()
{
	bool found = false;

#ifndef _WIN32
	if (task_meta_->sandbox->name == "isolate") {
		found = true;
	}
#endif

	if (found == false) {
		throw task_exception("Unknown sandbox type: " + task_meta_->sandbox->name);
	}
}

void external_task::sandbox_init()
{
#ifndef _WIN32
	if (task_meta_->sandbox->name == "isolate") {
		sandbox_ = std::make_shared<isolate_sandbox>(
			sandbox_config_, *limits_, worker_config_->get_worker_id(), temp_dir_, evaluation_dir_.string(), logger_);
	}
#endif
}

void external_task::sandbox_fini()
{
	sandbox_ = nullptr;
}

std::shared_ptr<task_results> external_task::run()
{
	sandbox_init();

	if (sandbox_ == nullptr) {
		// should never happen, unless we are doomed
		return nullptr;
	}

	// initialize output from stdout and stderr
	results_output_init();

	// check if binary is executable and set it otherwise
	make_binary_executable(task_meta_->binary);

	auto res = std::make_shared<task_results>();
	res->sandbox_status =
		std::unique_ptr<sandbox_results>(new sandbox_results(sandbox_->run(task_meta_->binary, task_meta_->cmd_args)));

	// get output from stdout and stderr
	get_results_output(res);

	sandbox_fini();

	// Check if sandbox ran successfully, else report error
	if (res->sandbox_status->status != isolate_status::OK) {
		res->status = task_status::FAILED;
		res->error_message = "Sandboxed program failed: " + res->sandbox_status->message;
	}

	return res;
}

std::shared_ptr<sandbox_limits> external_task::get_limits()
{
	return limits_;
}

void external_task::results_output_init()
{
	std::string random = helpers::random_alphanum_string(10);

	if (sandbox_config_->output || !sandbox_config_->carboncopy_stdout.empty()) {
		// output from stdout/stderr or carboncopy of stdout was requested
		if (sandbox_config_->std_output == "") {
			remove_stdout_ = true;
			std::string stdout_file = task_meta_->task_id + "." + random + ".output.stdout";
			sandbox_config_->std_output = (working_dir_ / fs::path(stdout_file)).string();
		}
	}

	if (sandbox_config_->output || !sandbox_config_->carboncopy_stderr.empty()) {
		// output from stdout/stderr or carboncopy of stderr was requested
		if (sandbox_config_->std_error == "") {
			remove_stderr_ = true;
			std::string stderr_file = task_meta_->task_id + "." + random + ".output.stderr";
			sandbox_config_->std_error = (working_dir_ / fs::path(stderr_file)).string();
		}
	}
}

fs::path external_task::find_path_outside_sandbox(std::string file)
{
	return helpers::find_path_outside_sandbox(
		file, sandbox_config_->chdir, limits_->bound_dirs, evaluation_dir_.string());
}

void external_task::get_results_output(std::shared_ptr<task_results> result)
{
	// files were outputted inside sandbox, so we have to find path outside sandbox
	fs::path stdout_file_path = find_path_outside_sandbox(sandbox_config_->std_output);
	fs::path stderr_file_path = find_path_outside_sandbox(sandbox_config_->std_error);
	process_results_output(result, stdout_file_path, stderr_file_path);

	// delete produced files if requested
	try {
		if (remove_stdout_) {
			fs::remove(stdout_file_path);
		}
		if (remove_stderr_) {
			fs::remove(stderr_file_path);
		}
	} catch (fs::filesystem_error &e) {
		logger_->warn("Temporary sandbox output files not cleaned properly: {}", e.what());
	}
}

void external_task::process_results_output(
	std::shared_ptr<task_results> result, fs::path stdout_path, fs::path stderr_path)
{
	if (sandbox_config_->output) {
		size_t max_length = worker_config_->get_max_output_length();
		std::string result_stdout(max_length, 0);
		std::string result_stderr(max_length, 0);

		// open and read files
		std::ifstream std_out(stdout_path.string());
		std::ifstream std_err(stderr_path.string());
		std_out.read(&result_stdout[0], max_length);
		std_err.read(&result_stderr[0], max_length);

		// if there was something in stdout, write it to result
		if (std_out.gcount() != 0) {
			result_stdout = result_stdout.substr(0, std_out.gcount());
			// filter non printable result
			helpers::filter_non_printable_chars(result_stdout);
			// write to result structure
			result->output_stdout = result_stdout;
		}

		// if there was something in stderr, write it to result
		if (std_err.gcount() != 0) {
			result_stderr = result_stderr.substr(0, std_err.gcount());
			// filter non printable result
			helpers::filter_non_printable_chars(result_stderr);
			// write to result structure
			result->output_stderr = result_stderr;
		}

		// be nice and close streams
		std_out.close();
		std_err.close();
	}
}

void external_task::process_carboncopy_output(fs::path stdout_path, fs::path stderr_path)
{
	size_t max_length = worker_config_->get_max_carboncopy_length();
	if (!sandbox_config_->carboncopy_stdout.empty()) {
		std::ifstream infile(stdout_path.string(), std::ios::binary);
		std::ofstream copy_file(sandbox_config_->carboncopy_stdout, std::ios::binary);

		// make the copying, by reading the file in memory and writing it to the copy
		std::string inmemory(max_length, 0);
		infile.read(&inmemory[0], max_length);
		copy_file.write(&inmemory[0], infile.gcount());

		// some day we do not close streams... but it is not this day
		infile.close();
		copy_file.close();
	}

	if (!sandbox_config_->carboncopy_stderr.empty()) {
		std::ifstream infile(stderr_path.string(), std::ios::binary);
		std::ofstream copy_file(sandbox_config_->carboncopy_stderr, std::ios::binary);

		// make the copying, by reading the file in memory and writing it to the copy
		std::string inmemory(max_length, 0);
		infile.read(&inmemory[0], max_length);
		copy_file.write(&inmemory[0], infile.gcount());

		// close streams, you fools
		infile.close();
		copy_file.close();
	}
}

void external_task::make_binary_executable(std::string binary)
{
	fs::path binary_path;
	try {
		binary_path = find_path_outside_sandbox(binary);
		if (binary_path.empty()) {
			logger_->info("Sandbox path {} not found in local filesystem, executable bit not set", binary);
			return;
		}

		// determine if file has executable bits set
		fs::file_status stat = status(binary_path);
		if (stat.permissions() & (fs::perms::owner_exe | fs::perms::group_exe | fs::perms::others_exe)) {
			return;
		}

		fs::permissions(
			binary_path, fs::perms::add_perms | fs::perms::owner_exe | fs::perms::group_exe | fs::others_exe);
	} catch (fs::filesystem_error &e) {
		log_and_throw(logger_,
			"Failed to set executable bits for path inside '",
			binary,
			"' and outside '",
			binary_path.string(),
			"'. Error: ",
			e.what());
	}
}
