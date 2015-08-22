/** Setup interface queuing desciplines for network emulation
 *
 * We use the firewall mark to apply individual netem qdiscs
 * per node. Every node uses an own BSD socket.
 * By using so SO_MARK socket option (see socket(7))
 * we can classify traffic originating from a node seperately.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _TC_H_
#define _TC_H_

#include <stdint.h>

#include <netlink/route/qdisc.h>
#include <netlink/route/classifier.h>

#include <libconfig.h>

typedef uint32_t tc_hdl_t;

struct interface;

/** Parse network emulator (netem) settings.
 *
 * @param cfg A libconfig object containing the settings.
 * @param ne A pointer to a libnl3 qdisc object where setting will be written to.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int tc_parse(config_setting_t *cfg, struct rtnl_qdisc *ne);

/** Print network emulator (netem) setting into buffer.
 *
 * @param buf A character buffer to write to.
 * @param len The length of the supplied buffer.
 * @param tc A pointer to the libnl3 qdisc object where settings will be read from.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int tc_print(char *buf, size_t len, struct rtnl_qdisc *ne);

/** Remove all queuing disciplines and filters.
 *
 * @param i The interface
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int tc_reset(struct interface *i);

/** Create a priority (prio) queueing discipline.
 *
 * @param i[in] The interface
 * @param qd[in,out] The libnl3 object of the new prio qdisc.
 * @param handle[in] The handle for the new qdisc
 * @param bands[in] The number of classes for this new qdisc
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int tc_prio(struct interface *i, struct rtnl_qdisc **qd, tc_hdl_t handle, int bands);

/** Add a new network emulator (netem) discipline.
 *
 * @param i[in] The interface to which this qdisc will be added.
 * @param qd[in,out] The libnl3 object of the new prio qdisc.
 * @param handle[in] The handle of the new qdisc.
 * @param parent[in] Make this qdisc a child of this class
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int tc_netem(struct interface *i, struct rtnl_qdisc **qd, tc_hdl_t handle, tc_hdl_t parent);

/** Add a new filter based on the netfilter mark.
 *
 * @param i The interface to which this classifier is applied to.
 * @param cls[in,out] The libnl3 object of the new prio qdisc.
 * @param flowid The destination class for matched traffic
 * @param mark The netfilter firewall mark (sometime called 'fwmark')
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
*/
int tc_mark(struct interface *i, struct rtnl_cls **cls, tc_hdl_t flowid, uint32_t mark);

#endif /* _TC_H_ */
