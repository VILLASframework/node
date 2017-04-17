/** Convert old style config to new JSON format.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <jansson.h>
#include <libconfig.h>

#include <villas/json.h>
#include <villas/utils.h>

void usage()
{
	printf("Usage: conf2json < input.conf > output.json\n\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret;
	config_t cfg;
	config_setting_t *cfg_root;
	json_t *json;
	
	if (argc != 1) {
		usage();
		exit(EXIT_FAILURE);
	}
	
	config_init(&cfg);
	
	ret = config_read(&cfg, stdin);
	if (ret != CONFIG_TRUE)
		return ret;
	
	cfg_root = config_root_setting(&cfg);
	
	json = config_to_json(cfg_root);
	if (!json)
		return -1;
	
	ret = json_dumpf(json, stdout, JSON_INDENT(2)); fflush(stdout);
	if (ret)
		return ret;
	
	json_decref(json);
	config_destroy(&cfg);
	
	return 0;
}
