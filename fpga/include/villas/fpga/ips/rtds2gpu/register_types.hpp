#ifndef REGISTER_TYPES_H
#define REGISTER_TYPES_H

#include <stdint.h>

union axilite_reg_status_t {
	uint32_t value;
	struct {
		uint32_t
		last_seq_nr			: 16,
		last_count			: 6,
		max_frame_size		: 6,
		invalid_frame_size	: 1,
		frame_too_short		: 1,
		frame_too_long		: 1,
		is_running			: 1;
	};
};

union reg_doorbell_t {
	uint32_t value;
	struct {
		uint32_t
		seq_nr			: 16,
		count			: 6,
		is_valid		: 1;
	};
};

#endif // REGISTER_TYPES_H
