#ifndef CODEX_WORKER_SANDBOX_CONFIG_H
#define CODEX_WORKER_SANDBOX_CONFIG_H

#include <map>
#include "sandbox_limits.h"


/**
 *
 */
class sandbox_config
{
public:
	std::string name = "";
	std::map<std::string, std::shared_ptr<sandbox_limits>> loaded_limits;
};

#endif // CODEX_WORKER_SANDBOX_CONFIG_H
