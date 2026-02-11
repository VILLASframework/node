/* Node type: File.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

#include <jansson.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

#include <villas/exceptions.hpp>
#include <villas/format.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/file.hpp>
#include <villas/queue.h>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

#include "villas/config_class.hpp"
#include "villas/sample.hpp"

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static char *file_format_name(const char *format, struct timespec *ts) {
  struct tm tm;
  char *buf = new char[FILE_MAX_PATHLEN];
  if (!buf)
    throw MemoryAllocationError();

  // Convert time
  gmtime_r(&ts->tv_sec, &tm);

  strftime(buf, FILE_MAX_PATHLEN, format, &tm);

  return buf;
}

static struct timespec file_calc_offset(const struct timespec *first,
                                        const struct timespec *epoch,
                                        enum file::EpochMode mode) {
  // Get current time
  struct timespec now = time_now();
  struct timespec offset;

  // Set offset depending on epoch
  switch (mode) {
  case file::EpochMode::DIRECT: // read first value at now + epoch
    offset = time_diff(first, &now);
    return time_add(&offset, epoch);

  case file::EpochMode::WAIT: // read first value at now + first + epoch
    offset = now;
    return time_add(&now, epoch);

  case file::EpochMode::RELATIVE: // read first value at first + epoch
    return *epoch;

  case file::EpochMode::ABSOLUTE: // read first value at f->epoch
    return time_diff(first, epoch);

  default:
    return (struct timespec){0};
  }
}

int villas::node::file_parse(NodeCompat *n, json_t *json) {
  auto *f = n->getData<struct file>();

  int ret;
  json_error_t err;
  json_t *json_format = nullptr;

  const char *uri_tmpl = nullptr;
  const char *eof = nullptr;
  const char *epoch = nullptr;
  const char *read_mode = nullptr;
  double epoch_flt = 0;

  ret = json_unpack_ex(json, &err, 0,
                       "{ s: s, s?: o, s?: { s?: s, s?: F, s?: s, s?: F, s?: "
                       "i, s?: i, s?: s }, s?: { s?: b, s?: i } }",
                       "uri", &uri_tmpl, "format", &json_format, "in", "eof",
                       &eof, "rate", &f->rate, "epoch_mode", &epoch, "epoch",
                       &epoch_flt, "buffer_size", &f->buffer_size_in, "skip",
                       &f->skip_lines, "read_mode", &read_mode, "out", "flush",
                       &f->flush, "buffer_size", &f->buffer_size_out);
  if (ret)
    throw ConfigError(json, err, "node-config-node-file");

  f->epoch = time_from_double(epoch_flt);
  f->uri_tmpl = uri_tmpl ? strdup(uri_tmpl) : nullptr;

  // Format
  if (f->formatter)
    delete f->formatter;
  f->formatter = json_format ? FormatFactory::make(json_format)
                             : FormatFactory::make("villas.human");
  if (!f->formatter)
    throw ConfigError(json_format, "node-config-node-file-format",
                      "Invalid format configuration");

  if (eof) {
    if (!strcmp(eof, "exit") || !strcmp(eof, "stop"))
      f->eof_mode = file::EOFBehaviour::STOP;
    else if (!strcmp(eof, "rewind"))
      f->eof_mode = file::EOFBehaviour::REWIND;
    else if (!strcmp(eof, "wait"))
      f->eof_mode = file::EOFBehaviour::SUSPEND;
    else
      throw RuntimeError("Invalid mode '{}' for 'eof' setting", eof);
  }

  if (epoch) {
    if (!strcmp(epoch, "direct"))
      f->epoch_mode = file::EpochMode::DIRECT;
    else if (!strcmp(epoch, "wait"))
      f->epoch_mode = file::EpochMode::WAIT;
    else if (!strcmp(epoch, "relative"))
      f->epoch_mode = file::EpochMode::RELATIVE;
    else if (!strcmp(epoch, "absolute"))
      f->epoch_mode = file::EpochMode::ABSOLUTE;
    else if (!strcmp(epoch, "original"))
      f->epoch_mode = file::EpochMode::ORIGINAL;
    else
      throw RuntimeError("Invalid value '{}' for setting 'epoch'", epoch);
  }

  if (read_mode) {
    if (!strcmp(read_mode, "all"))
      f->read_mode = file::ReadMode::READ_ALL;
    else if (!strcmp(read_mode, "rate_based"))
      f->read_mode = file::ReadMode::RATE_BASED;
    else
      throw RuntimeError("Invalid value '{}' for setting 'read_mode'",
                         read_mode);
  }

  json_t *json_uri = json_object_get(json, "uri");
  json_t *json_uris = json_object_get(json, "uris");

  if (json_uris && json_is_array(json_uris)) {
    f->multi_file_mode = true;
    size_t idx;
    json_t *json_uri_item;

    json_array_foreach(json_uris, idx, json_uri_item) {
      if (json_is_string(json_uri_item)) {
        const char *uri_str = json_string_value(json_uri_item);
        f->uri_templates.push_back(std::string(uri_str));
      }
    }
  } else if (json_uri && json_is_string(json_uri)) {
    f->multi_file_mode = false;
  } else
    throw ConfigError(json, "node-config-node-file",
                      "Must specify either 'uri' or 'uris'");

  return 0;
}

char *villas::node::file_print(NodeCompat *n) {
  auto *f = n->getData<struct file>();
  char *buf = nullptr;

  const char *epoch_str = nullptr;
  const char *eof_str = nullptr;
  const char *read_mode = nullptr;

  switch (f->epoch_mode) {
  case file::EpochMode::DIRECT:
    epoch_str = "direct";
    break;

  case file::EpochMode::WAIT:
    epoch_str = "wait";
    break;

  case file::EpochMode::RELATIVE:
    epoch_str = "relative";
    break;

  case file::EpochMode::ABSOLUTE:
    epoch_str = "absolute";
    break;

  case file::EpochMode::ORIGINAL:
    epoch_str = "original";
    break;

  default:
    epoch_str = "";
    break;
  }

  switch (f->eof_mode) {
  case file::EOFBehaviour::STOP:
    eof_str = "stop";
    break;

  case file::EOFBehaviour::SUSPEND:
    eof_str = "wait";
    break;

  case file::EOFBehaviour::REWIND:
    eof_str = "rewind";
    break;

  default:
    eof_str = "";
    break;
  }

  switch (f->read_mode) {
  case file::ReadMode::READ_ALL:
    read_mode = "all";
    break;

  case file::ReadMode::RATE_BASED:
    read_mode = "rate_based";
    break;

  default:
    read_mode = "";
    break;
  }

  strcatf(&buf,
          "uri=%s, out.flush=%s, in.skip=%d, in.eof=%s, in.epoch=%s, "
          "in.epoch=%.2f, in.read_mode=%s",
          f->uri ? f->uri : f->uri_tmpl, f->flush ? "yes" : "no", f->skip_lines,
          eof_str, epoch_str, time_to_double(&f->epoch), read_mode);

  if (f->rate)
    strcatf(&buf, ", in.rate=%.1f", f->rate);

  if (f->first.tv_sec || f->first.tv_nsec)
    strcatf(&buf, ", first=%.2f", time_to_double(&f->first));

  if (f->offset.tv_sec || f->offset.tv_nsec)
    strcatf(&buf, ", offset=%.2f", time_to_double(&f->offset));

  if ((f->first.tv_sec || f->first.tv_nsec) &&
      (f->offset.tv_sec || f->offset.tv_nsec)) {
    struct timespec eta, now = time_now();

    eta = time_add(&f->first, &f->offset);
    eta = time_diff(&now, &eta);

    if (eta.tv_sec || eta.tv_nsec)
      strcatf(&buf, ", eta=%.2f sec", time_to_double(&eta));
  }

  return buf;
}

int villas::node::file_start(NodeCompat *n) {
  auto *f = n->getData<struct file>();

  struct timespec now = time_now();
  int ret;

  // Prepare file name
  if (f->uri)
    delete[] f->uri;

  f->uri = file_format_name(f->uri_tmpl, &now);

  // Check if directory exists
  struct stat sb;
  char *cpy = strdup(f->uri);
  char *dir = dirname(cpy);

  ret = stat(dir, &sb);
  if (ret) {
    if (errno == ENOENT || errno == ENOTDIR) {
      ret = mkdir(dir, 0755);
      if (ret)
        throw SystemError("Failed to create directory");
    } else if (errno != EISDIR)
      throw SystemError("Failed to stat");
  } else if (!S_ISDIR(sb.st_mode)) {
    ret = mkdir(dir, 0644);
    if (ret)
      throw SystemError("Failed to create directory");
  }

  free(cpy);

  f->formatter->start(n->getInputSignals(false));

  //Experimental code for testing multi file read mode
  if (f->multi_file_mode && f->read_mode == file::ReadMode::READ_ALL) {
    struct timespec now = time_now();

    for (const auto &uri_tmpl : f->uri_templates) {
      char *resolved_uri = file_format_name(uri_tmpl.c_str(), &now);
      std::string filename = resolved_uri;
      delete[] resolved_uri;

      FILE *file_stream = fopen(filename.c_str(), "r");
      if (!file_stream) {
        n->logger->warn("Failed to open file: {}", filename);
        continue;
      }

      std::vector<Sample *> file_data;
      while (true) {
        Sample *smp = sample_alloc_mem(n->getInputSignals(false)->size());
        if (!smp) {
          n->logger->error("Failed to allocate sample for file: {}",
                           filename.c_str());
          break;
        }

        int ret = f->formatter->scan(file_stream, smp);
        if (ret < 0) {
          sample_free(smp);
          n->logger->error("Failed to scan file stream");
          break;
        }
        if (ret == 0) {
          sample_free(smp);
          n->logger->debug("Finished reating from file_stream");
          break;
        }

        file_data.push_back(smp);
      }

      f->file_samples[filename] = std::move(file_data);
      f->file_read_pos[filename] = 0;
      f->uris.push_back(filename);
    }

    for (auto &file_sample_pair : f->file_samples) {
      const std::vector<Sample *> &current_file = file_sample_pair.second;
      f->total_files_size += current_file.size();
    }
    f->read_pos = 0;

    return 0;
  }

  // Open file
  f->stream_out = fopen(f->uri, "a+");
  if (!f->stream_out)
    return -1;

  f->stream_in = fopen(f->uri, "r");
  if (!f->stream_in)
    return -1;

  if (f->buffer_size_in) {
    ret = setvbuf(f->stream_in, nullptr, _IOFBF, f->buffer_size_in);
    if (ret)
      return ret;
  }

  if (f->buffer_size_out) {
    ret = setvbuf(f->stream_out, nullptr, _IOFBF, f->buffer_size_out);
    if (ret)
      return ret;
  }

  // Create timer
  f->task.setRate(f->rate);

  // Get timestamp of first line
  if (f->epoch_mode != file::EpochMode::ORIGINAL) {
    rewind(f->stream_in);

    if (feof(f->stream_in)) {
      n->logger->warn("Empty file");
    } else {
      struct Sample smp;

      smp.capacity = 0;

      ret = f->formatter->scan(f->stream_in, &smp);
      if (ret == 1) {
        f->first = smp.ts.origin;
        f->offset = file_calc_offset(&f->first, &f->epoch, f->epoch_mode);
      } else
        n->logger->warn("Failed to read first timestamp");
    }
  }

  rewind(f->stream_in);

  // Fast-forward
  struct Sample *smp = sample_alloc_mem(n->getInputSignals(false)->size());
  for (unsigned i = 0; i < f->skip_lines; i++)
    f->formatter->scan(f->stream_in, smp);

  sample_free(smp);

  if (f->read_mode == file::ReadMode::READ_ALL) {
    Sample *smp;
    int ret;

    while (!feof(f->stream_in)) {
      smp = sample_alloc_mem(n->getInputSignals(false)->size());
      if (!smp) {
        n->logger->error("Failed to allocate samples");
        break;
      }

      ret = f->formatter->scan(f->stream_in, smp);
      if (ret < 0) {
        sample_free(smp);
        break;
      }

      f->samples.push_back(smp);
    }

    f->read_pos = 0;
  }
  return 0;
}

int villas::node::file_stop(NodeCompat *n) {
  auto *f = n->getData<struct file>();
  n->logger->info("Stopping node {}", n->getName());
  // Experimental code to implement multi file read
  if (f->multi_file_mode) {
    for (auto &[filename, samples] : f->file_samples) {
      for (auto smp : samples) {
        sample_free(smp);
      }
    }
    f->file_samples.clear();
    f->file_read_pos.clear();
    f->uris.clear();
  }

  for (auto smp : f->samples) {
    sample_free(smp);
  }
  f->samples.clear();

  f->task.stop();

  fclose(f->stream_in);
  fclose(f->stream_out);

  return 0;
}
int villas::node::file_read(NodeCompat *n, struct Sample *const smps[],
                            unsigned cnt) {
  auto *f = n->getData<struct file>();
  // size_t file_pos = 0;

  // Experimental code to implement multi file read
  if (f->multi_file_mode && f->read_mode == file::ReadMode::READ_ALL) {
    unsigned read_count = 0;

    //Trying sequential read

    for (const auto &filename : f->uris) {
      if (read_count >= cnt)
        break;
      auto &samples = f->file_samples[filename];
      size_t &pos = f->file_read_pos[filename];
      while (pos < samples.size() && read_count < cnt) {
        sample_copy(smps[read_count], samples[pos++]);
        f->read_pos++;
        read_count++;
      }
    }

    if (f->read_pos == f->total_files_size) {
      n->logger->info("Reached end of buffer");
      n->setState(State::STOPPING);
      return -1;
    }
    return (read_count > 0) ? read_count : -1;
  }

  int ret;
  uint64_t steps;

  assert(cnt == 1);

  if (f->read_mode == file::ReadMode::READ_ALL) {
    unsigned read_count = 0;
    while (f->read_pos < f->samples.size() && read_count < cnt) {

      sample_copy(smps[read_count], f->samples[f->read_pos++]);
      if (f->epoch_mode == file::EpochMode::ORIGINAL) {
        //No waiting
      } else if (f->rate) {
        steps = f->task.wait();
        if (steps == 0)
          throw SystemError("Failed to wait for timer");
        else if (steps != 1)
          n->logger->warn("Missed steps: {}", steps - 1);

        smps[read_count]->ts.origin = time_now();
      } else {
        smps[read_count]->ts.origin =
            time_add(&smps[read_count]->ts.origin, &f->offset);
        f->task.setNext(&smps[read_count]->ts.origin);

        steps = f->task.wait();
        if (steps == 0)
          throw SystemError("Failed to wait for timer");
        else if (steps != 1)
          n->logger->warn("Missed steps: {}", steps - 1);
      }
      read_count++;
    }

    if (f->read_pos == f->samples.size()) {
      switch (f->eof_mode) {
      case file::EOFBehaviour::REWIND:
        n->logger->info("Rewind input file");

        f->read_pos = 0;
        break;

      case file::EOFBehaviour::SUSPEND:
        usleep(100000); // 100ms sleep

        // Try to download more data if this is a remote file.
        clearerr(f->stream_in);
        break;

      case file::EOFBehaviour::STOP:
      default:
        n->logger->info("Reached end of buffer");
        n->setState(State::STOPPING);
        return -1;
      }
      return -1;
    }

    return read_count;
  }

retry:
  ret = f->formatter->scan(f->stream_in, smps, cnt);
  if (ret <= 0) {
    if (feof(f->stream_in)) {
      switch (f->eof_mode) {
      case file::EOFBehaviour::REWIND:
        n->logger->info("Rewind input file");

        f->offset = file_calc_offset(&f->first, &f->epoch, f->epoch_mode);
        rewind(f->stream_in);
        goto retry;

      case file::EOFBehaviour::SUSPEND:
        // We wait 10ms before fetching again.
        usleep(100000);

        // Try to download more data if this is a remote file.
        clearerr(f->stream_in);
        goto retry;

      case file::EOFBehaviour::STOP:
        n->logger->info("Reached end-of-file.");

        n->setState(State::STOPPING);

        return -1;

      default: {
      }
      }
    } else
      n->logger->warn("Failed to read messages: reason={}", ret);

    return 0;
  }

  // We dont wait in FILE_EPOCH_ORIGINAL mode
  if (f->epoch_mode == file::EpochMode::ORIGINAL)
    return cnt;

  if (f->rate) {
    steps = f->task.wait();

    smps[0]->ts.origin = time_now();
  } else {
    smps[0]->ts.origin = time_add(&smps[0]->ts.origin, &f->offset);

    f->task.setNext(&smps[0]->ts.origin);
    steps = f->task.wait();
  }

  // Check for overruns
  if (steps == 0)
    throw SystemError("Failed to wait for timer");
  else if (steps != 1)
    n->logger->warn("Missed steps: {}", steps - 1);

  return cnt;
}

int villas::node::file_write(NodeCompat *n, struct Sample *const smps[],
                             unsigned cnt) {
  int ret;
  auto *f = n->getData<struct file>();

  assert(cnt == 1);

  ret = f->formatter->print(f->stream_out, smps, cnt);
  if (ret < 0)
    return ret;

  if (f->flush)
    fflush(f->stream_out);

  return cnt;
}

int villas::node::file_poll_fds(NodeCompat *n, int fds[]) {
  auto *f = n->getData<struct file>();

  if (f->rate) {
    fds[0] = f->task.getFD();

    return 1;
  } else if (f->epoch_mode == file::EpochMode::ORIGINAL) {
    fds[0] = fileno(f->stream_in);

    return 1;
  }

  return -1; // TODO: not supported yet
}

int villas::node::file_init(NodeCompat *n) {
  auto *f = n->getData<struct file>();

  // We require a real-time clock here as we can sync against the
  // timestamps in the file.
  new (&f->task) Task(CLOCK_REALTIME);
  new (&f->file_samples) decltype(f->file_samples)(); // std::map ctor
  new (&f->file_read_pos) decltype(f->file_read_pos)();
  new (&f->uris) decltype(f->uris)();

  // Default values
  f->rate = 0;
  f->eof_mode = file::EOFBehaviour::STOP;
  f->epoch_mode = file::EpochMode::DIRECT;
  f->flush = 0;
  f->buffer_size_in = 0;
  f->buffer_size_out = 0;
  f->skip_lines = 0;
  f->read_mode = file::ReadMode::RATE_BASED;
  f->read = false;
  f->total_files_size = 0;
  f->formatter = nullptr;

  return 0;
}

int villas::node::file_destroy(NodeCompat *n) {
  auto *f = n->getData<struct file>();

  f->task.~Task();

  if (f->uri)
    delete[] f->uri;

  if (f->formatter)
    delete f->formatter;

  return 0;
}

static NodeCompatType p;

__attribute__((constructor(110))) static void register_plugin() {
  p.name = "file";
  p.description = "support for file log / replay node type";
  p.vectorize = 1;
  p.size = sizeof(struct file);
  p.init = file_init;
  p.destroy = file_destroy;
  p.parse = file_parse;
  p.print = file_print;
  p.start = file_start;
  p.stop = file_stop;
  p.read = file_read;
  p.write = file_write;
  p.poll_fds = file_poll_fds;

  static NodeCompatFactory ncp(&p);
}
