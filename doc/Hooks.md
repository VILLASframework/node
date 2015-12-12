# Hooks

Hooks are simple callback functions which are called whenever a message is processed by a path.

There are several built-in hooks for:
  - collecting, show & reset statistics
  - drop reordered messages
  - verify message headers
  - handle simulation restarts

But main goal of this mechanism is to provide extensibility for the end user.
Example applications for hooks might be:

 1. Filter sample values
 2. Manipulate sample values: FIR
 3. Transform sample values: FFT, DCT
 4. Update network emulation settings based on sample values

## Configuration

Each path is allowed to have multiple active hooks.
Those can be configured in the path section using the `hooks` setting:

	paths = [
		{
			in = "input_node",
			out = "output_node",
			hooks = [ "fir", "print" ]
	#		hooks = "decimate"    // alternative syntax for a single hook
		}
	]

**Please note:** the hooks will be executed in the order they are given in the configuration file!

## API

The interface for user-defined hook functions is defined in (`server/include/hooks.h`):

```
typedef int (*hook_cb_t)(struct path *p, struct hook *h, int when);
```

There are several occasions when a hook can be executed in the program flow (@see hook_type).
There is a dedicated queue for each occasion.

The queue for which a hook can be called is encoded as a mask and specified during registration of that hook. 

| Queue			| Desciption								|
| --------------------- | --------------------------------------------------------------------- |
| `HOOK_PATH_START`	| Called whenever a path is started; before threads are created.	|
| `HOOK_PATH_STOP`	| Called whenever a path is stopped; after threads are destoyed.	|
| `HOOK_PATH_RESTART`	| Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. |
| `HOOK_PRE`		| Called when a new packet of messages (samples) was received.		|
| `HOOK_POST`		| Called after each message (sample) of a packet was processed.		|
| `HOOK_MSG`		| Called for each message (sample) in a packet.				|
| `HOOK_PERIODIC`	| Called periodically. Period is set by global 'stats' option in the configuration file. |

## Examples

There are already several hooks defined in (server/src/hooks.c).
Use one of those as a starting point for your own hook.

A typical hook function might look like this one:

	int my_super_hook_function(struct path *p, struct hook *h, int when)
	{
		struct msg *last_received_msg = p->current;
	
		printf("The last message contained %u values\n", last_received_msg->length);
	}
