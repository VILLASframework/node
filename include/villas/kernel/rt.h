/** Linux specific real-time optimizations
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#ifndef _RT_H_
#define _RT_H_

int rt_init(int affinity, int priority);

#endif /* _RT_H_ */