# Development

Developement is currently coordinated by Steffen Vogel <stvogel@eonerc.rwth-aachen.de> using [GitLab](http://git.rwth-aachen.de/VILLASframework/VILLASnode).
Please feel free to submit pull requests or bug reports.

@todo Add link to contribution guidelines

## Programming Paradigm

VILLASnode is currently written in C using the ISO C99 standard.
Yet, it is heavily structured into modules / plugins and uses a C++-like object oriented style.
In the future, VILLASnode might switch to lightweight C++.

Main _classes_ in VILLASnode are struct node, struct path, struct hook and struct api_ressource.
In order to track the life cycle of those objects, each of them has an enum state member.
The following figure illustrates the state machine which is used:

@diafile states.dia

## Shared library: libvillas

VILLASnode is split into a shared library called libvillas and a couple of executables (`villas-node`, `villas-pipe`, `villas-test`, `villas-signal`, ...) which are linked against this library.

## Extensibilty / Plugins

There are many places where VILLASnode can easily extended with plugins:

### Example of new node type

See `include/villas/plugin.h`

See `lib/nodes/file.c`:

	[...]
	
	static struct plugin p = {
		.name		= "file",
		.description	= "support for file log / replay node type",
		.type		= PLUGIN_TYPE_NODE,
		.node		= {
			.vectorize	= 1,
			.size		= sizeof(struct file),
			.reverse	= file_reverse,
			.parse		= file_parse,
			.print		= file_print,
			.start		= file_start,
			.stop		= file_stop,
			.read		= file_read,
			.write		= file_write,
			.instances	= LIST_INIT()
		}
	};
	
	REGISTER_PLUGIN(&p)