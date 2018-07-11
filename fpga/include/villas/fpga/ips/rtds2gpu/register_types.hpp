#ifndef REGISTER_TYPES_H
#define REGISTER_TYPES_H

#include <stdint.h>
#include <cstddef>

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

template<size_t N, typename T = uint32_t>
struct Rtds2GpuMemoryBuffer {
	static constexpr size_t valueCount = N;
	static constexpr size_t dataOffset = offsetof(Rtds2GpuMemoryBuffer, data);
	static constexpr size_t doorbellOffset = offsetof(Rtds2GpuMemoryBuffer, doorbell);

	T data[N];
	reg_doorbell_t doorbell;
} __attribute__((packed));

#endif // REGISTER_TYPES_H
