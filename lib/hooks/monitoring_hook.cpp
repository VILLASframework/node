#include <villas/hook.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/timing.hpp>
#include <chrono>

/*
 * This Hook replaces the values of the signals by the metrics: Rate, Delay, lost and out-of-order sequences
 */

namespace villas {
    namespace node {
        class MonitoringHook : public Hook {
            protected:
                double rate;
                uint64_t total_received; // total received samples
                bool initial;
                std::chrono::steady_clock::time_point start, now;
                uint64_t initial_seq;  // the initial sequence received
                uint64_t total_missed; // the total number of missed sequences
                uint64_t highest_seq;  // the number of the last sequence
                uint64_t total_oo;     // the total number of out of order sequences
                int64_t delay;         // delay in us

            public:
                MonitoringHook(Path *p, Node *n, int fl, int prio, bool en = true)
                    : Hook(p, n, fl, prio, en), rate(0), total_received(0), initial(false), initial_seq(0),
                    total_missed(0), highest_seq(0), total_oo(0) {}

                void prepare() override {

                    assert(state == State::CHECKED);

                    Hook::prepare();

                    auto rate_sig = std::make_shared<Signal>("rate", "", SignalType::FLOAT);
                    if (!rate_sig)
                        throw RuntimeError("Failed to create new signal: rate");
                    signals->insert(signals->begin(), rate_sig);

                    auto out_of_order_sig = std::make_shared<Signal>("out_of_order_sequences", "", SignalType::INTEGER);
                    if (!out_of_order_sig)
                        throw RuntimeError("Failed to create new signal: total out of order sequeces");
                    signals->insert(signals->begin(), out_of_order_sig);

                    auto missed_sig = std::make_shared<Signal>("missed_sequences", "", SignalType::INTEGER);
                    if (!missed_sig)
                        throw RuntimeError("Failed to create new signal: missed sequences");
                    signals->insert(signals->begin(), missed_sig);

                    auto delay_sig = std::make_shared<Signal>("delay", "", SignalType::INTEGER);
                    if (!delay_sig)
                        throw RuntimeError("Failed to create new signal: delay");
                    signals->insert(signals->begin(), delay_sig);

                    state = State::PREPARED;
                }

                Hook::Reason process(struct Sample *smp) override {

                    assert(state == State::STARTED);

                    // used to set initial variables
                    if (!initial) {
                        initial = true;
                        total_received += 1;
                        start = std::chrono::steady_clock::now();
                        initial_seq = smp->sequence;
                        highest_seq = smp->sequence;
                        total_missed = 0;
                        total_oo = 0;
                    } else {

                        total_received += 1;
                        now = std::chrono::steady_clock::now();
                        std::chrono::duration<double> elapsed_seconds{now - start};

                        rate = (total_received - 1) / elapsed_seconds.count();

                        if (smp->sequence > highest_seq) {
                            if (smp->sequence != highest_seq + 1)
                            {
                                total_missed += smp->sequence - highest_seq - 1;
                            }
                            highest_seq = smp->sequence;
                        } else {
                            if (initial_seq > smp->sequence)
                            {
                                total_oo += 1;
                            } else {
                                total_missed -= 1;
                                total_oo += 1;
                            }
                        }
                    }

                    // logic to calculate delay same as jitter_calc.cpp

                    timespec now = time_now();
                    int64_t delay_sec, delay_nsec;
                    delay_sec = now.tv_sec - smp->ts.origin.tv_sec;
                    delay_nsec = now.tv_nsec - smp->ts.origin.tv_nsec;

                    // delay in us
                    delay = delay_sec * 1000000 + delay_nsec / 1000;

                    // remove the signals from the samples
                    sample_data_remove(smp, 0, smp->length);
                    sample_data_insert(smp, (union SignalData *)&rate, 0, 1);
                    sample_data_insert(smp, (union SignalData *)&total_oo, 0, 1);
                    sample_data_insert(smp, (union SignalData *)&total_missed, 0, 1);
                    sample_data_insert(smp, (union SignalData *)&delay, 0, 1);

                    return Reason::OK;
                }
        };
        static char n[] = "monitoring_hook";
        static char d[] = "overrides the signals with metrics";
        static HookPlugin<MonitoringHook, n, d,
                          (int)Hook::Flags::PATH | (int)Hook::Flags::NODE_READ |
                              (int)Hook::Flags::NODE_WRITE>
            p;

    }
}