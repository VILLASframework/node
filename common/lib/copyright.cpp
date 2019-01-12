/** General purpose helper functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#include <iostream>

#include <villas/copyright.hpp>
#include <villas/colors.hpp>
#include <villas/config.h>

void villas::print_copyright()
{
	std::cout << PROJECT_NAME " " << CLR_BLU(PROJECT_BUILD_ID)
	          << " (built on " CLR_MAG(__DATE__) " " CLR_MAG(__TIME__) ")" << std::endl
	          << " Copyright 2014-2017, Institute for Automation of Complex Power Systems, EONERC" << std::endl
	          << " Steffen Vogel <StVogel@eonerc.rwth-aachen.de>" << std::endl;
}

void villas::print_version()
{
	std::cout << PROJECT_BUILD_ID << std::endl;
}
