#include <memory>
#include <algorithm>
#include <string>
#include <map>
#include <vector>

#include "config.h"


/**
 * Properly add an exit-code or an interval of exit-codes in the bitmap.
 */
void add_exit_codes(std::vector<bool> &success_exit_codes, int from_index, int to_index = -1)
{
	if (to_index == -1) { to_index = from_index; }

	if (from_index < 0 || to_index > 255 || from_index > to_index) { return; }

	if (success_exit_codes.size() <= ((std::size_t) to_index)) { success_exit_codes.resize(to_index + 1); }

	for (int i = from_index; i <= to_index; ++i) { success_exit_codes[i] = true; }
}


/**
 * Process the config node with success exit codes and fill a bitmap with their enabled indices.
 * The config must be a single int value or a list of values. In case of a list, each item should be either integer or a
 * tuple of two integers representing from-to range (inclusive).
 * @param node root of yaml sub-tree with the exit codes.
 * @param success_exit_codes a bitmap being constructed
 */
void load_task_success_exit_codes(const YAML::Node &node, std::vector<bool> &success_exit_codes)
{
	success_exit_codes.clear();
	if (node.IsScalar()) {
		add_exit_codes(success_exit_codes, node.as<int>());
	} else if (node.IsSequence()) {
		for (auto &subnode : node) {
			if (subnode.IsScalar()) {
				add_exit_codes(success_exit_codes, subnode.as<int>());
			} else if (subnode.IsSequence() && subnode.size() == 2) {
				add_exit_codes(success_exit_codes, subnode[0].as<int>(), subnode[1].as<int>());
			} else {
				throw helpers::config_exception(
					"Success exit code must be a scalar (int) value or an interval (two integers in a list)");
			}
		}
	} else {
		throw helpers::config_exception("Task command success-exit-codes must be an integer or a list.");
	}
}


std::shared_ptr<job_metadata> helpers::build_job_metadata(const YAML::Node &conf)
{
	std::shared_ptr<job_metadata> job_meta = std::make_shared<job_metadata>();

	try {
		// initial checkouts
		if (!conf.IsDefined()) {
			throw config_exception("Job config file was empty");
		} else if (!conf.IsMap()) {
			throw config_exception("Job configuration is not a map");
		} else if (!conf["tasks"]) {
			throw config_exception("Item tasks was not given in job configuration");
		} else if (!conf["submission"]) {
			throw config_exception("Item submission was not given in job configuration");
		} else if (!conf["tasks"].IsSequence()) {
			throw config_exception("Item tasks in job configuration is not sequence");
		} else if (!conf["submission"].IsMap()) {
			throw config_exception("Item submission in job configuration is not map");
		}


		// get information about this submission
		auto submiss = conf["submission"];
		if (submiss["job-id"] && submiss["job-id"].IsScalar()) {
			job_meta->job_id = submiss["job-id"].as<std::string>();
		} else {
			throw config_exception("Submission.job-id item not loaded properly");
		}
		if (submiss["file-collector"] && submiss["file-collector"].IsScalar()) {
			job_meta->file_server_url = submiss["file-collector"].as<std::string>();
		} else {
			throw config_exception("Submission.file-collector item not loaded properly");
		}
		if (submiss["log"] && submiss["log"].IsScalar()) {
			job_meta->log = submiss["log"].as<bool>();
		} // can be omitted... no throw
		if (submiss["hw-groups"] && submiss["hw-groups"].IsSequence()) {
			job_meta->hwgroups = submiss["hw-groups"].as<std::vector<std::string>>();
		} else {
			throw config_exception("Submission.hw-groups item not loaded properly");
		}


		// load datas for tasks and save them
		for (auto &ctask : conf["tasks"]) {
			std::shared_ptr<task_metadata> task_meta = std::make_shared<task_metadata>();

			if (ctask["task-id"] && ctask["task-id"].IsScalar()) {
				task_meta->task_id = ctask["task-id"].as<std::string>();
			} else {
				throw config_exception("Configuration task has missing task-id");
			}
			if (ctask["priority"] && ctask["priority"].IsScalar()) {
				task_meta->priority = ctask["priority"].as<std::size_t>();
			} else {
				task_meta->priority = 1; // default value
			}
			if (ctask["fatal-failure"] && ctask["fatal-failure"].IsScalar()) {
				task_meta->fatal_failure = ctask["fatal-failure"].as<bool>();
			} else {
				task_meta->fatal_failure = false; // default value
			}
			if (ctask["cmd"]) {
				if (ctask["cmd"].IsMap()) {
					if (ctask["cmd"]["bin"] && ctask["cmd"]["bin"].IsScalar()) {
						task_meta->binary = ctask["cmd"]["bin"].as<std::string>();
					} else {
						throw config_exception("Runnable binary for task not given");
					}

					if (ctask["cmd"]["args"] && ctask["cmd"]["args"].IsSequence()) {
						task_meta->cmd_args = ctask["cmd"]["args"].as<std::vector<std::string>>();
					} // can be omitted... no throw

					if (ctask["cmd"]["success-exit-codes"]) {
						load_task_success_exit_codes(ctask["cmd"]["success-exit-codes"], task_meta->success_exit_codes);
					}
				} else {
					throw config_exception("Command in task is not a map");
				}
			} else {
				throw config_exception("Configuration of one task has missing cmd");
			}
			if (ctask["test-id"] && ctask["test-id"].IsScalar()) {
				task_meta->test_id = ctask["test-id"].as<std::string>();
			}

			// load dependencies
			if (ctask["dependencies"] && ctask["dependencies"].IsSequence()) {
				task_meta->dependencies = ctask["dependencies"].as<std::vector<std::string>>();
			}

			// load task type
			if (ctask["type"] && ctask["type"].IsScalar()) {
				task_meta->type = helpers::get_task_type(ctask["type"].as<std::string>());
			}

			// distinguish internal/external command and construct suitable object
			if (ctask["sandbox"]) {

				// //////////////// //
				// external command //
				// //////////////// //

				std::shared_ptr<sandbox_config> sandbox = std::make_shared<sandbox_config>();

				if (ctask["sandbox"]["name"] && ctask["sandbox"]["name"].IsScalar()) {
					sandbox->name = ctask["sandbox"]["name"].as<std::string>();
				} else {
					throw config_exception("Name of sandbox not given");
				}

				if (ctask["sandbox"]["stdin"] && ctask["sandbox"]["stdin"].IsScalar()) {
					sandbox->std_input = ctask["sandbox"]["stdin"].as<std::string>();
				} // can be ommited... no throw
				if (ctask["sandbox"]["stdout"] && ctask["sandbox"]["stdout"].IsScalar()) {
					sandbox->std_output = ctask["sandbox"]["stdout"].as<std::string>();
				} // can be ommited... no throw
				if (ctask["sandbox"]["stderr"] && ctask["sandbox"]["stderr"].IsScalar()) {
					sandbox->std_error = ctask["sandbox"]["stderr"].as<std::string>();
				} // can be ommited... no throw
				if (ctask["sandbox"]["stderr-to-stdout"] && ctask["sandbox"]["stderr-to-stdout"].IsScalar()) {
					sandbox->stderr_to_stdout = ctask["sandbox"]["stderr-to-stdout"].as<bool>();
				} // can be ommited... no throw
				if (ctask["sandbox"]["output"] && ctask["sandbox"]["output"].IsScalar()) {
					sandbox->output = ctask["sandbox"]["output"].as<bool>();
				} // can be ommited... no throw
				if (ctask["sandbox"]["carboncopy-stdout"] && ctask["sandbox"]["carboncopy-stdout"].IsScalar()) {
					sandbox->carboncopy_stdout = ctask["sandbox"]["carboncopy-stdout"].as<std::string>();
				} // can be ommited... no throw
				if (ctask["sandbox"]["carboncopy-stderr"] && ctask["sandbox"]["carboncopy-stderr"].IsScalar()) {
					sandbox->carboncopy_stderr = ctask["sandbox"]["carboncopy-stderr"].as<std::string>();
				} // can be ommited... no throw
				if (ctask["sandbox"]["chdir"] && ctask["sandbox"]["chdir"].IsScalar()) {
					sandbox->chdir = ctask["sandbox"]["chdir"].as<std::string>();
				} // can be ommited... no throw
				if (ctask["sandbox"]["working-directory"] && ctask["sandbox"]["working-directory"].IsScalar()) {
					sandbox->working_directory = ctask["sandbox"]["working-directory"].as<std::string>();
				} // can be ommited... no throw

				// load limits... if they are supplied
				if (ctask["sandbox"]["limits"]) {
					if (!ctask["sandbox"]["limits"].IsSequence()) {
						throw config_exception("Sandbox limits are not sequence");
					}

					for (auto &lim : ctask["sandbox"]["limits"]) {
						std::shared_ptr<sandbox_limits> sl = std::make_shared<sandbox_limits>();
						std::string hwgroup;

						if (lim["hw-group-id"] && lim["hw-group-id"].IsScalar()) {
							hwgroup = lim["hw-group-id"].as<std::string>();
						} else {
							throw config_exception("Hwgroup ID not defined in sandbox limits");
						}

						if (lim["time"] && lim["time"].IsScalar()) {
							sl->cpu_time = lim["time"].as<float>();
						} else {
							sl->cpu_time = FLT_MAX; // set undefined value (max float)
						}
						if (lim["wall-time"] && lim["wall-time"].IsScalar()) {
							sl->wall_time = lim["wall-time"].as<float>();
						} else {
							sl->wall_time = FLT_MAX; // set undefined value (max float)
						}
						if (lim["extra-time"] && lim["extra-time"].IsScalar()) {
							sl->extra_time = lim["extra-time"].as<float>();
						} else {
							sl->extra_time = FLT_MAX; // set undefined value (max float)
						}
						if (lim["stack-size"] && lim["stack-size"].IsScalar()) {
							sl->stack_size = lim["stack-size"].as<std::size_t>();
						} else {
							sl->stack_size = SIZE_MAX; // set undefined value (max std::size_t)
						}
						if (lim["memory"] && lim["memory"].IsScalar()) {
							sl->memory_usage = lim["memory"].as<std::size_t>();
						} else {
							sl->memory_usage = SIZE_MAX; // set undefined value (max std::size_t)
						}
						if (lim["extra-memory"] && lim["extra-memory"].IsScalar()) {
							sl->extra_memory = lim["extra-memory"].as<std::size_t>();
						} else {
							sl->extra_memory = SIZE_MAX; // set undefined value (max std::size_t)
						}
						if (lim["parallel"] && lim["parallel"].IsScalar()) { // TODO not defined properly
							sl->processes = lim["parallel"].as<std::size_t>();
						} else {
							sl->processes = SIZE_MAX; // set undefined value (max std::size_t)
						}
						if (lim["disk-quotas"] && lim["disk-quotas"].IsScalar()) {
							sl->disk_quotas = lim["disk-quotas"].as<bool>();
						} else {
							sl->disk_quotas = false;
						}
						if (lim["disk-size"] && lim["disk-size"].IsScalar()) {
							sl->disk_size = lim["disk-size"].as<std::size_t>();
						} else {
							sl->disk_size = SIZE_MAX; // set undefined value (max std::size_t)
						}
						if (lim["disk-files"] && lim["disk-files"].IsScalar()) {
							sl->disk_files = lim["disk-files"].as<std::size_t>();
						} else {
							sl->disk_files = SIZE_MAX; // set undefined value (max std::size_t)
						}

						// find bound dirs from config and attach them to limits
						auto bound_dirs = helpers::get_bind_dirs(lim);
						sl->add_bound_dirs(bound_dirs);

						if (lim["environ-variable"] && lim["environ-variable"].IsMap()) {
							for (auto &var : lim["environ-variable"]) {
								sl->environ_vars.emplace_back(
									var.first.as<std::string>(), var.second.as<std::string>());
							}
						}

						sandbox->loaded_limits.insert(std::make_pair(hwgroup, sl));
					}
				}

				task_meta->sandbox = sandbox;
			} else {

				// //////////////// //
				// internal command //
				// //////////////// //

				// nothing to do here right now
			}

			// add task to job_meta
			job_meta->tasks.push_back(task_meta);
		}

	} catch (YAML::Exception &e) {
		throw config_exception("Exception in yaml-cpp: " + std::string(e.what()));
	}

	return job_meta;
}

task_type helpers::get_task_type(const std::string &type)
{
	std::string lower = type;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	if (lower == "evaluation") {
		return task_type::EVALUATION;
	} else if (lower == "execution") {
		return task_type::EXECUTION;
	} else if (lower == "initiation") {
		return task_type::INITIATION;
	}

	return task_type::INNER;
}

std::vector<std::tuple<std::string, std::string, sandbox_limits::dir_perm>> helpers::get_bind_dirs(
	const YAML::Node &lim)
{
	std::vector<std::tuple<std::string, std::string, sandbox_limits::dir_perm>> bound_dirs;

	if (lim["bound-directories"] && lim["bound-directories"].IsSequence()) {
		for (auto &dir : lim["bound-directories"]) {
			std::string src;
			std::string dst;
			sandbox_limits::dir_perm mode = sandbox_limits::dir_perm::RO;
			if (dir.IsMap()) {
				if (dir["mode"] && dir["mode"].IsScalar()) {
					std::string str_mode = dir["mode"].as<std::string>();
					std::transform(str_mode.begin(), str_mode.end(), str_mode.begin(), ::tolower);

					for (const auto &kv : sandbox_limits::get_dir_perm_associated_strings()) {
						if (str_mode.find(kv.second) != std::string::npos) {
							mode = static_cast<sandbox_limits::dir_perm>(mode | kv.first);
						}
					}

					if (mode & sandbox_limits::dir_perm::TMP) { // special checks for tmp
						if (mode & sandbox_limits::dir_perm::FS) {
							throw config_exception(
								"Options 'fs' and 'tmp' are incompatible (they cannot be used together)");
						}
						if (dir["src"]) {
							throw config_exception(
								"Path 'src' must not be present when mounting 'tmp' directory (only 'dst')");
						}
					}
				}

				if (dir["src"] && dir["src"].IsScalar()) { dst = src = dir["src"].as<std::string>(); }
				if (dir["dst"] && dir["dst"].IsScalar()) {
					dst = dir["dst"].as<std::string>();
					if (src.empty()) { src = dst; }
				}
				if (src.empty() || dst.empty()) {
					throw config_exception("Either 'src' or 'dst' must be defined in every 'bound-directories' record");
				}

				bound_dirs.emplace_back(src, dst, mode);
			}
		}
	} // can be omitted... no throw

	return bound_dirs;
}
