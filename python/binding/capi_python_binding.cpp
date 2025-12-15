/* Python-binding.
 *
 * Author: Kevin Vu te Laar <vu.te@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power
 * Systems, RWTH Aachen University SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>
#include <optional>
#include <stdexcept>

#include <jansson.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include <villas/node.hpp>
#include <villas/sample.hpp>
#include <villas/timing.hpp>

extern "C" {
#include <villas/node.h>
}

namespace py = pybind11;

class SamplesArray {
public:
  SamplesArray(unsigned int len = 0) {
    smps = (len > 0) ? new vsample *[len]() : nullptr;
    this->len = len;
  }
  SamplesArray(const SamplesArray &) = delete;
  SamplesArray &operator=(const SamplesArray &) = delete;

  ~SamplesArray() {
    if (!smps)
      return;
    for (unsigned int i = 0; i < len; ++i) {
      if (smps[i]) {
        sample_decref(smps[i]);
        smps[i] = nullptr;
      }
    }
    delete[] smps;
  }

  void *get_block(unsigned int start) { return (void *)&smps[start]; }

  void bulk_alloc(unsigned int start_idx, unsigned int stop_idx,
                  unsigned int smpl_len) {
    for (unsigned int i = start_idx; i < stop_idx; ++i) {
      if (smps[i]) {
        sample_decref(smps[i]);
      }
      smps[i] = sample_alloc(smpl_len);
    }
  }

  /*
   * Performs a resize of the underlying SamplesArray copying each Sample.
   * Shrinking has asymmetric behavior which may be undesired.
   * Therefore use clear().
   */
  int grow(unsigned int add) {
    unsigned int new_len = this->len + add;
    vsample **smps_new = new vsample *[new_len]();
    for (unsigned int i = 0; i < this->len; ++i) {
      smps_new[i] = smps[i];
    }
    delete[] smps;
    this->smps = smps_new;
    this->len = new_len;

    return new_len;
  }

  int clear() {
    if (this->smps) {
      unsigned int i = 0;
      for (; i < len; ++i) {
        sample_decref(smps[i]);
        smps[i] = nullptr;
      }
      delete[] smps;
      smps = nullptr;
      this->len = 0;
      return i;
    }
    return -1;
  }

  vsample *&operator[](unsigned int idx) {
    vsample *&ref = smps[idx];
    return ref;
  }

  vsample *operator[](unsigned int idx) const { return smps[idx]; }

  vsample **get_smps() { return smps; }

  unsigned int size() const { return len; }

private:
  vsample **smps;
  unsigned int len;
};

struct timespec ns_to_timespec(int64_t time_ns) {
  struct timespec ts;
  ts.tv_nsec = time_ns / 1'000'000'000LL;
  ts.tv_sec = time_ns % 1'000'000'000LL;
  return ts;
}

/* pybind11 can not deal with (void **) as function input parameters,
 * therefore cast a simple (void *) pointer to the corresponding type
 *
 * wrapper bindings, sorted alphabetically
 * @param villas_node   Name of the module to be bound
 * @param m             Access variable for modifying the module code
 */
PYBIND11_MODULE(python_binding, m) {
  m.def("memory_init", &memory_init);

  m.def("node_check", [](void *n) -> int { return node_check((vnode *)n); });

  m.def("node_destroy",
        [](void *n) -> int { return node_destroy((vnode *)n); });

  m.def(
      "node_details",
      [](void *n) -> const char * { return node_details((vnode *)n); },
      py::return_value_policy::copy);

  m.def("node_input_signals_max_cnt", [](void *n) -> unsigned {
    return node_input_signals_max_cnt((vnode *)n);
  });

  m.def("node_is_enabled",
        [](void *n) -> bool { return node_is_enabled((const vnode *)n); });

  m.def("node_is_valid_name",
        [](const char *name) -> bool { return node_is_valid_name(name); });

  m.def(
      "node_name",
      [](void *n) -> const char * { return node_name((vnode *)n); },
      py::return_value_policy::copy);

  m.def(
      "node_name_full",
      [](void *n) -> const char * { return node_name_full((vnode *)n); },
      py::return_value_policy::copy);

  m.def(
      "node_name_short",
      [](void *n) -> const char * { return node_name_short((vnode *)n); },
      py::return_value_policy::copy);

  m.def(
      "node_new",
      [](const char *json_str, const char *id_str) -> vnode * {
        json_error_t err;
        uuid_t id;

        uuid_parse(id_str, id);
        auto *json = json_loads(json_str, 0, &err);

        void *it = json_object_iter(json);
        json_t *inner = json_object_iter_value(it);

        if (json_is_object(inner)) { // create node with name
          return (vnode *)villas::node::NodeFactory::make(
              json_object_iter_value(it), id, json_object_iter_key(it));
        } else { // create node without name
          char *capi_str = json_dumps(json, 0);
          auto ret = node_new(id_str, capi_str);

          free(capi_str);
          return ret;
        }
      },
      py::return_value_policy::take_ownership);

  m.def("node_output_signals_max_cnt", [](void *n) -> unsigned {
    return node_output_signals_max_cnt((vnode *)n);
  });

  m.def("node_pause", [](void *n) -> int { return node_pause((vnode *)n); });

  m.def("node_prepare",
        [](void *n) -> int { return node_prepare((vsample *)n); });

  m.def("node_read", [](void *n, SamplesArray &a, unsigned cnt) -> int {
    return node_read((vnode *)n, a.get_smps(), cnt);
  });

  m.def("node_read", [](void *n, void *smpls, unsigned cnt) -> int {
    return node_read((vnode *)n, (vsample **)smpls, cnt);
  });

  m.def("node_restart",
        [](void *n) -> int { return node_restart((vnode *)n); });

  m.def("node_resume", [](void *n) -> int { return node_resume((vnode *)n); });

  m.def("node_reverse",
        [](void *n) -> int { return node_reverse((vnode *)n); });

  m.def("node_start", [](void *n) -> int { return node_start((vnode *)n); });

  m.def("node_stop", [](void *n) -> int { return node_stop((vnode *)n); });

  m.def("node_to_json_str", [](void *n) -> py::str {
    auto json = reinterpret_cast<villas::node::Node *>(n)->toJson();
    char *json_str = json_dumps(json, 0);
    auto py_str = py::str(json_str);

    json_decref(json);
    free(json_str);

    return py_str;
  });

  m.def("node_write", [](void *n, SamplesArray &a, unsigned cnt) -> int {
    return node_write((vnode *)n, a.get_smps(), cnt);
  });

  m.def("node_write", [](void *n, void *smpls, unsigned cnt) -> int {
    return node_write((vnode *)n, (vsample **)smpls, cnt);
  });

  m.def(
      "smps_array",
      [](unsigned int len) -> SamplesArray * { return new SamplesArray(len); },
      py::return_value_policy::take_ownership);

  m.def("sample_alloc",
        [](unsigned int len) -> vsample * { return sample_alloc(len); });

  // Decrease reference count and release memory if last reference was held.
  m.def("sample_decref", [](void *smps) -> void {
    auto smp = (vsample **)smps;
    sample_decref(*smp);
  });

  m.def("sample_length", [](void *smp) -> unsigned {
    if (smp) {
      return sample_length((vsample *)smp);
    } else {
      return -1;
    }
  });

  m.def(
      "sample_pack",
      [](void *s, std::optional<int64_t> ts_origin_ns,
         std::optional<int64_t> ts_received_ns) -> vsample * {
        struct timespec ts_origin =
            ts_origin_ns ? ns_to_timespec(*ts_origin_ns) : time_now();
        struct timespec ts_received =
            ts_received_ns ? ns_to_timespec(*ts_received_ns) : time_now();

        auto smp = (villas::node::Sample *)s;
        uint64_t *seq = &smp->sequence;
        unsigned len = smp->length;
        double *values = (double *)smp->data;

        return sample_pack(seq, &ts_origin, &ts_received, len, values);
      },
      py::return_value_policy::reference);

  m.def(
      "sample_pack",
      [](const py::list values, std::optional<int64_t> ts_origin_ns,
         std::optional<int64_t> ts_received_ns, unsigned seq = 0) -> void * {
        struct timespec ts_origin =
            ts_origin_ns ? ns_to_timespec(*ts_origin_ns) : time_now();
        struct timespec ts_received =
            ts_received_ns ? ns_to_timespec(*ts_received_ns) : time_now();

        unsigned values_len = values.size();
        double cvalues[values.size()];
        for (unsigned int i = 0; i < values_len; ++i) {
          cvalues[i] = values[i].cast<double>();
        }
        uint64_t sequence = seq;

        auto tmp = (void *)sample_pack(&sequence, &ts_origin, &ts_received,
                                       values_len, cvalues);
        return tmp;
      },
      py::return_value_policy::reference);

  m.def(
      "sample_unpack",
      [](void *ss, void *ds, std::optional<int64_t> ts_origin_ns,
         std::optional<int64_t> ts_received_ns) -> void {
        struct timespec ts_origin =
            ts_origin_ns ? ns_to_timespec(*ts_origin_ns) : time_now();
        struct timespec ts_received =
            ts_received_ns ? ns_to_timespec(*ts_received_ns) : time_now();

        auto srcSmp = (villas::node::Sample **)ss;
        auto destSmp = (villas::node::Sample **)ds;

        if (!*srcSmp) {
          throw std::runtime_error("Tried to unpack empty sample!");
        }
        if (!*destSmp) {
          *destSmp = (villas::node::Sample *)sample_alloc((*srcSmp)->length);
        } else if ((*destSmp)->capacity < (*srcSmp)->length) {
          sample_decref(*(vsample **)destSmp);
          *destSmp = (villas::node::Sample *)sample_alloc((*srcSmp)->length);
        }

        uint64_t *seq = &(*destSmp)->sequence;
        int *flags = &(*destSmp)->flags;
        unsigned *len = &(*destSmp)->length;
        double *values = (double *)(*destSmp)->data;

        sample_unpack(*(vsample **)srcSmp, seq, &ts_origin, &ts_received, flags,
                      len, values);
      },
      py::return_value_policy::reference);

  m.def("sample_details", [](void *s) {
    auto smp = (villas::node::Sample *)s;
    if (!smp) {
      return py::dict();
    }

    py::dict d;
    d["sequence"] = smp->sequence;
    d["length"] = smp->length;
    d["capacity"] = smp->capacity;
    d["flags"] = smp->flags;
    d["refcnt"] = smp->refcnt.load();
    d["ts_origin"] = time_to_double(&smp->ts.origin);
    d["ts_received"] = time_to_double(&smp->ts.received);

    py::list data;
    for (unsigned int i = 0; i < smp->length; ++i) {
      data.append((double)smp->data[i]);
    }
    d["data"] = data;

    return d;
  });

  py::class_<SamplesArray>(m, "SamplesArray")
      .def(py::init<unsigned int>(), py::arg("len"))
      .def("__getitem__",
           [](SamplesArray &a, unsigned int idx) {
             assert(idx < a.size() && "Index out of bounds");
             return a[idx];
           })
      .def("__setitem__",
           [](SamplesArray &a, unsigned int idx, void *smp) {
             assert(idx < a.size() && "Index out of bounds");
             if (a[idx]) {
               sample_decref(a[idx]);
             }
             a[idx] = (vsample *)smp;
           })
      .def("__len__", &SamplesArray::size)
      .def("bulk_alloc", &SamplesArray::bulk_alloc)
      .def("grow", &SamplesArray::grow)
      .def("get_block", &SamplesArray::get_block)
      .def("clear", &SamplesArray::clear);
}
