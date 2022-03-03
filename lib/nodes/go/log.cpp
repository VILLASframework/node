#include <villas/log.hpp>

extern "C" {
	#include "log.h"
}

using namespace villas;

void log(char *logger, int level, char *msg);
{
	auto logger = logging.get(logger);

	spdlog::log(level, "{}", msg);

	free(logger);
	free(msg);
}
