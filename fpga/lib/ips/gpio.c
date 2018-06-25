/** GPIO related helper functions
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2018, Daniel Krebs
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include "config.h"
#include "plugin.h"

#include "fpga/ip.h"
#include "fpga/card.h"

static int gpio_start(struct fpga_ip *c)
{
	(void) c;

	return 0;
}

static struct plugin p = {
	.name		= "Xilinx's GPIO controller",
	.description	= "",
	.type		= PLUGIN_TYPE_FPGA_IP,
	.ip		= {
	    .vlnv	= { "xilinx.com", "ip", "axi_gpio", NULL },
		.type	= FPGA_IP_TYPE_MISC,
	    .start	= gpio_start,
	    .size	= 0
	}
};

REGISTER_PLUGIN(&p)
