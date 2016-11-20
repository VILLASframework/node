#include <villas/hook.h>
#include <villas/log.h>

static int hook_example(struct hook *h, int when, struct hook_info *j)
{
	info("Hello world from example hook!");
	
	return 0;
}

REGISTER_HOOK("example", "This is just a simple example hook", 99, 0, hook_example, HOOK_PATH_START)