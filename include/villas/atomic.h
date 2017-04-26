/** Workaround for differently named atomic types in C/C++
 *
 * @file
 * @author Georg Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#ifdef __cplusplus

#include <atomic>

typedef std::atomic_int atomic_int;
typedef std::atomic_size_t atomic_size_t;

#else

#include <stdatomic.h>

#endif
