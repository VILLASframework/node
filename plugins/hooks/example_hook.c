#include <villas/hooks.h>
#include <villas/log.h>

static int hook_example(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	info("Hello world from example hook!");
	
	return 0;
}

REGISTER_HOOK("example", "This is just a simple example hook", 99, 0, hook_example, HOOK_PATH_START)