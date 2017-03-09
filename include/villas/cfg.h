/** Configuration file parser.
 *
 * The server program is configured by a single file.
 * This config file is parsed with a third-party library:
 *  libconfig http://www.hyperrealm.com/libconfig/
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#ifndef _CFG_H_
#define _CFG_H_

#include <libconfig.h>

/* Forward declarations */
struct list;
struct settings;

/* Compatibility with libconfig < 1.5 */
#if (LIBCONFIG_VER_MAJOR <= 1) && (LIBCONFIG_VER_MINOR < 5)
  #define config_setting_lookup config_lookup_from
#endif

/** Simple wrapper around libconfig's config_init()
  *
  * This allows us to avoid an additional library dependency to libconfig
  * for the excuctables. They only have to depend on libvillas.
  */
void cfg_init(config_t *cfg);

/** Simple wrapper around libconfig's config_init() */
void cfg_destroy(config_t *cfg);

/** Parse config file and store settings in supplied struct settings.
 *
 * @param filename The path to the configration file (relative or absolute)
 * @param cfg A initialized libconfig object
 * @param set The global configuration structure
 * @param nodes A linked list of nodes which should be parsed
 * @param paths A linked list of paths which should be parsed
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int cfg_parse(const char *filename, config_t *cfg, struct settings *set,
	struct list *nodes, struct list *paths);

#endif /* _CFG_H_ */
