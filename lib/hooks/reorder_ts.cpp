/** Reorder samples hook.
 *
 * @author Philipp Jungkamp <philipp.jungkamp@opal-rt.com>
 * @copyright 2023, OPAL-RT Germany GmbH
 * @license Apache 2.0
 *********************************************************************************/

#include <cinttypes>
#include <cstring>
#include <ctime>
#include <vector>

#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class ReorderTsHook : public Hook {

protected:
	std::vector<Sample *> window;
	std::size_t window_size;
	Sample *buffer;

	bool compareTimestamp(Sample *lhs, Sample *rhs) {
		auto lhs_ts = lhs->ts.origin;
		auto rhs_ts = rhs->ts.origin;

		if (lhs_ts.tv_sec < rhs_ts.tv_sec)
			return true;

		if (lhs_ts.tv_sec == rhs_ts.tv_sec && lhs_ts.tv_nsec < rhs_ts.tv_nsec)
			return true;

		return false;
	}

	void swapSample(Sample *lhs, Sample *rhs) {
		if (buffer) {
			sample_copy(buffer, lhs);
			sample_copy(lhs, rhs);
			sample_copy(rhs, buffer);
		} else {
			buffer = sample_clone(lhs);
			sample_copy(lhs, rhs);
			sample_copy(rhs, buffer);
		}
	}

public:
	ReorderTsHook(Path *p, Node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		window{},
		window_size(16),
		buffer(nullptr)
	{ }

	virtual void parse(json_t *json)
	{
		assert(state != State::STARTED);

		json_error_t err;
		int ret = json_unpack_ex(json, &err, 0, "{ s?: i }",
			"window", &window_size
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-reorder-ts");

		state = State::PARSED;
	}

	virtual void start()
	{
		assert(state == State::PREPARED || state == State::STOPPED);

		window.reserve(window_size);

		state = State::STARTED;
	}

	virtual void stop()
	{
		assert(state == State::STARTED);

		for (auto sample : window)
			sample_free(sample);

		if (buffer)
			sample_free(buffer);

		window.clear();

		state = State::STOPPED;
	}

	virtual Hook::Reason process(Sample *sample)
	{
		assert(state == State::STARTED);
		assert(sample);

		if (window.empty()) {
			window.push_back(sample_clone(sample));

			logger->info("window.size={}/{}", window.size(), window_size);

			return Hook::Reason::SKIP_SAMPLE;
		}

		for (std::size_t i = window.size() - 1;; i--) {
			if (!compareTimestamp(sample, window[i])) {
				if (i != window.size() - 1) {
					logger->warn("Fixing reordered Sample");
					sample_dump(logger, sample);
				}

				if (window.size() == window_size) {
					// The front sample will be returned.
					Sample *window_sample = window.front();
					// Move all elements before the index of insertion towards the front.
					std::memmove(&window[0], &window[1], i * sizeof(decltype(window)::value_type));
					// Store the new sample at the correct index.
					window[i] = window_sample;
					// Swap the contents of the front sample with the processed sample.
					swapSample(window_sample, sample);

					return Hook::Reason::OK;
				} else {
					// Increase the vector size by 1.
					window.push_back(nullptr);
					// Move all elements from the index of insertion onwards towards the back.
					std::memmove(&window[i + 2], &window[i + 1], (window.size() - i - 2) * sizeof(decltype(window)::value_type));
					// Store the new sample at the correct index.
					window[i + 1] = sample_clone(sample);

					logger->info("window.size={}/{}", window.size(), window_size);

					return Hook::Reason::SKIP_SAMPLE;
				}
			}

			if (!i) break;
		}

		logger->error("Could not reorder Sample");
		sample_dump(logger, sample);

		return Hook::Reason::SKIP_SAMPLE;
	}

	virtual void restart()
	{
		assert(state == State::STARTED);

		for (auto sample : window)
			sample_free(sample);

		window.clear();
	}
};

/* Register hook */
static char n[] = "reorder_ts";
static char d[] = "Reorder messages by their timestamp";
static HookPlugin<ReorderTsHook, n, d, (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH | (int) Hook::Flags::NODE_READ, 2> p;

} /* namespace node */
} /* namespace villas */
