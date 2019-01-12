/** Various helper functions.
 *
 * @file
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

#pragma once

/* CPP stringification */
#define XSTR(x)		STR(x)
#define  STR(x)		#x

/* Some color escape codes for pretty log messages */
#define CLR(clr, str)	"\e[" XSTR(clr) "m" str "\e[0m"
#define CLR_GRY(str)	CLR(30, str) /**< Print str in gray */
#define CLR_RED(str)	CLR(31, str) /**< Print str in red */
#define CLR_GRN(str)	CLR(32, str) /**< Print str in green */
#define CLR_YEL(str)	CLR(33, str) /**< Print str in yellow */
#define CLR_BLU(str)	CLR(34, str) /**< Print str in blue */
#define CLR_MAG(str)	CLR(35, str) /**< Print str in magenta */
#define CLR_CYN(str)	CLR(36, str) /**< Print str in cyan */
#define CLR_WHT(str)	CLR(37, str) /**< Print str in white */
#define CLR_BLD(str)	CLR( 1, str) /**< Print str in bold */
