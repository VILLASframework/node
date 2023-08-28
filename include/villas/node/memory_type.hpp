/** Memory allocators.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

namespace memory {

struct Type;

typedef struct Allocation * (*allocator_t)(size_t len, size_t alignment, struct Type *mem);
typedef int (*deallocator_t)(struct Allocation * ma, struct Type *mem);

enum class Flags {
	MMAP		= (1 << 0),
	DMA		= (1 << 1),
	HUGEPAGE	= (1 << 2),
	HEAP		= (1 << 3)
};

struct Type {
	const char *name;
	int flags;

	size_t alignment;

	allocator_t alloc;
	deallocator_t free;

	void *_vd; /**< Virtual data for internal state */
};

extern struct Type heap;
extern struct Type mmap;
extern struct Type mmap_hugetlb;
extern struct Type *default_type;

struct Type * ib(NodeCompat *n, struct Type *parent);
struct Type * managed(void *ptr, size_t len);

int mmap_init(int hugepages) __attribute__ ((warn_unused_result));

} // namespace memory
} // namespace node
} // namespace villas
