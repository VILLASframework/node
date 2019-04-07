/** A sliding/moving window.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <stdlib.h>

#include <villas/window.h>
#include <villas/utils.h>

int window_init(struct window *w, size_t steps, double init)
{
	size_t len = LOG2_CEIL(steps);

	/* Allocate memory for ciruclar history buffer */
	w->data = (double *) alloc(len * sizeof(double));
	if (!w->data)
		return -1;

	for (size_t i = 0; i < len; i++)
		w->data[i] = init;

	w->steps = steps;
	w->pos = len;
	w->mask = len - 1;

	return 0;
}

int window_destroy(struct window *w)
{
	free(w->data);

	return 0;
}

double window_update(struct window *w, double in)
{
	double out = w->data[(w->pos - w->steps) & w->mask];

	w->data[w->pos++ & w->mask] = in;

	return out;
}
