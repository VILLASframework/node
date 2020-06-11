#pragma once

#include <stdint.h>
#include <cstddef>
#include <cstdint>

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

	constexpr reg_doorbell_t() : value(0) {}
};

template<size_t N, typename T = uint32_t>
struct Rtds2GpuMemoryBuffer {
	// this type is only for memory interpretation, it makes no sense to create
	// an instance so it's forbidden
	Rtds2GpuMemoryBuffer() = delete;

	// T can be a more complex type that wraps multiple values
	static constexpr size_t rawValueCount = N * (sizeof(T) / 4);

	// As of C++14, offsetof() is not working for non-standard layout types (i.e.
	// composed of non-POD members). This might work in C++17 though.
	// More info: https://gist.github.com/graphitemaster/494f21190bb2c63c5516
	//static constexpr size_t doorbellOffset = offsetof(Rtds2GpuMemoryBuffer, doorbell);
	//static constexpr size_t dataOffset = offsetof(Rtds2GpuMemoryBuffer, data);

	// HACK: This might break horribly, let's just hope C++17 will be there soon
	static constexpr size_t dataOffset = 0;
	static constexpr size_t doorbellOffset = N * sizeof(Rtds2GpuMemoryBuffer::data);

	T data[N];
	reg_doorbell_t doorbell;
};
