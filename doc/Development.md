# Development

Developement is currently coordinated by Steffen Vogel <stvogel@eonerc.rwth-aachen.de> using [GitHub](http://github.com/RWTH-ACS/S2SS).
Please feel free to submit pull requests or bug reports.

## Extensibilty

There are two main places where S2SS can easily extended:

#### New node-type

    REGISTER_NODE_TYPE(struct node_type *vt)

#### New hook functions

    /** The type of a hook defines when a hook will be exectuted. This is used as a bitmask. */
    enum hook_type {
    	HOOK_PATH_START		= 1 << 0, /**< Called whenever a path is started; before threads are created. */
	
	[...]
	
    	HOOK_ASYNC		= 1 << 6, /**< Called asynchronously with fixed rate (see path::rate). */
	
    	HOOK_PERIODIC		= 1 << 7, /**< Called periodically. Period is set by global 'stats' option in the configuration file. */
	
    	/** @{ Classes of hooks */
    	/** Internal hooks are mandatory. */
    	HOOK_INTERNAL		= 1 << 16,
    	/** All hooks */
    	HOOK_ALL		= HOOK_INTERNAL - 1
    	/** @} */
    };
    
    typedef int (*hook_cb_t)(struct path *p, struct hook *h, int when);
    
    REGISTER_HOOK(char *name, int priority, hook_cb_t cb, enum hook_type when)
