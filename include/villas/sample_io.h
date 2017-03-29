/** Read / write sample data in different formats.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include <stdio.h>
#include <jansson.h>

/* Forward declarations */
struct sample;

enum sample_io_format {
	SAMPLE_IO_FORMAT_VILLAS,
	SAMPLE_IO_FORMAT_JSON,
	SAMPLE_IO_FORMAT_HDF5,
	SAMPLE_IO_FORMAT_COMTRADE
};

/** These flags define the format which is used by sample_io_fscan() and sample_io_fprint(). */
enum sample_flags {
	SAMPLE_IO_NANOSECONDS	= (1 << 0),
	SAMPLE_IO_OFFSET	= (1 << 1),
	SAMPLE_IO_SEQUENCE	= (1 << 2),
	SAMPLE_IO_VALUES	= (1 << 3),
	SAMPLE_IO_ALL		= 16-1
};

/* Not implemented yet */
#if 0
struct sample_io_handle {
	enum sample_io_format format;
	int flags

	FILE *file;

	struct list fields;
};

int sample_io_init(struct sample_io *io, enum sample_io_format fmt, char *mode, int flags);

int sample_io_destroy(struct sample_io *io);

int sample_io_open(struct sample_io *io);

int sample_io_close(struct sample_io *io);

int sample_io_write(struct sample_io *io, struct sample *smps[], size_t cnt);

int sample_io_read(struct sample_io *io, struct sample *smps[], size_t cnt);

int sample_io_eof(struct sample_io *io);
int sample_io_rewind(struct sample_io *io);
#endif

/* Lowlevel interface */

int sample_io_fprint(FILE *f, struct sample *s, enum sample_io_format fmt, int flags);

int sample_io_fscan(FILE *f, struct sample *s, enum sample_io_format fmt, int *flags);

/* VILLASnode human readable format */

int sample_io_villas_print(char *buf, size_t len, struct sample *s, int flags);

int sample_io_villas_scan(const char *line, struct sample *s, int *fl);

int sample_io_villas_fprint(FILE *f, struct sample *s, int flags);

int sample_io_villas_fscan(FILE *f, struct sample *s, int *flags);

/* JSON format */

int sample_io_json_pack(json_t **j, struct sample *s, int flags);

int sample_io_json_unpack(json_t *j, struct sample *s, int *flags);

int sample_io_json_fprint(FILE *f, struct sample *s, int flags);

int sample_io_json_fscan(FILE *f, struct sample *s, int *flags);
