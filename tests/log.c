/** Unit tests for log functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include "log.h"
#include "utils.h"

struct param {
	char *expression;
	long expected;
};

static struct log l;

static void init()
{
	log_init(&l, V, LOG_ALL);
}

static void fini()
{
	log_destroy(&l);
}

ParameterizedTestParameters(log, facility_expression)
{
	static struct param params[] = {
		{ "all,!pool",		LOG_ALL & ~LOG_POOL },
		{ "pool,!pool",		LOG_POOL & ~LOG_POOL },
		{ "pool,nodes,!socket",	(LOG_POOL | LOG_NODES) & ~LOG_SOCKET },
		{ "kernel",		LOG_KERNEL },
		{ "ngsi",		LOG_NGSI },
		{ "all",		LOG_ALL },
		{ "!all",		0 },
		{ "",			0 }
	};
	
	return cr_make_param_array(struct param, params, ARRAY_LEN(params));
}

ParameterizedTest(struct param *p, log, facility_expression, .init = init, .fini = fini)
{
	log_set_facility_expression(&l, p->expression);
	
	cr_assert_eq(l.facilities, p->expected, "log.faciltities is %#lx not %#lx", l.facilities, p->expected);
}