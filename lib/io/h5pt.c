/** HDF5 Packet Table IO format based-on the H5PT high-level API.
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

#include <stdlib.h>

#include <hdf5_hl.h>

#include "plugin.h"
#include "io.h"

enum h5pt_flags {
	FORMAT_H5PT_COMPRESS = (1 << 16)
};

struct h5pt {
	hsize_t index;
	hsize_t num_packets;

	hid_t fid;	/**< File identifier. */
	hid_t ptable;	/**< Packet table identifier. */
}

hid_t h5pt_create_datatype(int values)
{
	hid_t dt = H5Tcreate(H5T_COMPOUND, sizeof());
}

int h5pt_open(struct io *io, const char *uri, const char *mode)
{
	struct h5pt *h5 = io->_vd;

	herr_t err;

	/* Create a file using default properties */
	h5->fid = H5Fcreate(uri, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

	/* Create a fixed-length packet table within the file */
	/* This table's "packets" will be simple integers. */
	h5->ptable = H5PTcreate_fl(fid, "Packet Test Dataset", H5T_NATIVE_INT, 1, 1);
	if (h5->ptable == H5I_INVALID_HID)
		return -1;

	err = H5PTis_valid(h5->ptable);
	if (err < 0)
		return err;

	err = H5PTget_num_packets(h5->ptable, h5->num_packets);
	if (err < 0)
		return err;

	io_format_h5pt_rewind(io);

	return 0;
}

int h5pt_close(struct io *io)
{
	struct h5pt *h5 = io->_vd;

	herr_t err;

	/* Close dataset */
	err = H5PTclose(h5->ptable);
	if (err < 0)
		goto out;

	/* Close file */
out:	err = H5Fclose(h5->fid);
	if (err < 0)
		return err;

	return 0;
}

int h5pt_rewind(struct io *io)
{
	struct h5pt *h5 = io->_vd;

	herr_t err;

	err = H5PTcreate_index(h5->ptable);
	if (err < 0)
		return err;

	h5->index = 0;

	return 0;
}

int h5pt_eof(struct io *io)
{
	struct h5pt *h5 = io->_vd;

	return h5->index >= h5->num_packets;
}

int h5pt_flush(struct io *io)
{
	struct h5pt *h5 = io->_vd;

	herr_t err;

	err = H5Fflush(h5->fid, H5F_SCOPE_GLOBAL);
	if (err < 0)
		return err;

	return 0;
}

int h5pt_print(struct io *io, struct sample *smps[], size_t cnt)
{
	struct h5pt *h5 = io->_vd;

	herr_t err;

	/* Write one packet to the packet table */
	err = H5PTappend(h5->ptable, smps, cnt);
	if (err < 0)
		goto out;

	return 0;
}

int h5pt_scan(struct io *io, struct sample *smps[], size_t cnt)
{
	struct h5pt *h5 = io->_vd;

	herr_t err;

	H5PTget_next(h5->ptable, cnt, smps)

	return 0;
}

static struct plugin p = {
	.name = "hdf5",
	.description = "HDF5 Packet Table",
	.type = PLUGIN_TYPE_FORMAT,
	.format = {
		.open	= h5pt_open,
		.close	= h5pt_close,
		.rewind	= h5pt_rewind,
		.eof	= h5pt_eof,
		.flush	= h5pt_flush,
		.print	= h5pt_print,
		.scan	= h5pt_scan,

		.flags	= IO_FORMAT_BINARY,
		.size = sizeof(struct h5pt)
	}
};
