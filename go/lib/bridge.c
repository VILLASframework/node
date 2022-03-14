#include <stdlib.h>

#include <villas/nodes/go.h>

void bridge_go_register_node_factory(_go_register_node_factory_cb cb, _go_plugin_list pl, char *name, char *desc, int flags)
{
    cb(pl, name, desc, flags);
}

void bridge_go_logger_log(_go_logger_log_cb cb, _go_logger l, int level, char *msg)
{
    cb(l, level, msg);
    free(msg);
}
