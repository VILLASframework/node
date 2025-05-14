/* Python-binding.
 *
 * Author: Kevin Vu te Laar <vu.te@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power
 * Systems, RWTH Aachen University SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>
#include <pybind11/pybind11.h>
#include <uuid/uuid.h>

#include <villas/node.hpp>
#include <villas/sample.hpp>

extern "C" {
#include <villas/node.h>
}

namespace py = pybind11;

class Array {
public:
  Array(unsigned int len) {
    smps = new vsample *[len]();
    this->len = len;
  }
  Array(const Array &) = delete;
  Array &operator=(const Array &) = delete;

  ~Array() {
    for (unsigned int i = 0; i < len; ++i) {
      sample_decref(smps[i]);
      smps[i] = nullptr;
    }
    delete[] smps;
  }

  vsample *&operator[](unsigned int idx) { return smps[idx]; }

  vsample *&operator[](unsigned int idx) const { return smps[idx]; }

  vsample **get_smps() { return smps; }

  unsigned int size() const { return len; }

private:
  vsample **smps;
  unsigned int len;
};

/* pybind11 can not deal with (void **) as function input parameters,
 * therefore we have to cast a simple (void *) pointer to the corresponding type
 *
 * wrapper bindings, sorted alphabetically
 * @param villas_node   Name of the module to be bound
 * @param m             Access variable for modifying the module code
 */
PYBIND11_MODULE(villas_node, m) {
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

  m.def("node_netem_fds", [](void *n, int fds[]) -> int {
    return node_netem_fds((vnode *)n, fds);
  });

  m.def(
      "node_new",
      [](const char *id_str, const char *json_str) -> vnode * {
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
      py::return_value_policy::reference);

  m.def("node_output_signals_max_cnt", [](void *n) -> unsigned {
    return node_output_signals_max_cnt((vnode *)n);
  });

  m.def("node_pause", [](void *n) -> int { return node_pause((vnode *)n); });

  m.def("node_poll_fds", [](void *n, int fds[]) -> int {
    return node_poll_fds((vnode *)n, fds);
  });

  m.def("node_prepare",
        [](void *n) -> int { return node_prepare((vsample *)n); });

  m.def("node_read", [](void *n, Array &a, unsigned cnt) -> int {
    return node_read((vnode *)n, a.get_smps(), cnt);
  });

  m.def("node_restart",
        [](void *n) -> int { return node_restart((vnode *)n); });

  m.def("node_resume", [](void *n) -> int { return node_resume((vnode *)n); });

  m.def("node_reverse",
        [](void *n) -> int { return node_reverse((vnode *)n); });

  m.def("node_start", [](void *n) -> int { return node_start((vnode *)n); });

  m.def("node_stop", [](void *n) -> int { return node_stop((vnode *)n); });

  m.def("node_to_json", [](void *n) -> py::str {
    auto json = reinterpret_cast<villas::node::Node *>(n)->toJson();
    char *json_str = json_dumps(json, 0);
    auto py_str = py::str(json_str);

    json_decref(json);
    free(json_str);

    return py_str;
  });

  m.def("node_to_json_str", [](void *n) -> py::str {
    auto json = reinterpret_cast<villas::node::Node *>(n)->toJson();
    char *json_str = json_dumps(json, 0);
    auto py_str = py::str(json_str);

    json_decref(json);
    free(json_str);

    return py_str;
  });

  m.def("node_write", [](void *n, Array &a, unsigned cnt) -> int {
    return node_write((vnode *)n, a.get_smps(), cnt);
  });

  m.def(
      "smps_array", [](unsigned int len) -> Array * { return new Array(len); },
      py::return_value_policy::take_ownership);

  m.def("sample_alloc",
        [](unsigned int len) -> vsample * { return sample_alloc(len); });

  // Decrease reference count and release memory if last reference was held.
  m.def("sample_decref", [](void *smps) -> void {
    auto smp = (vsample **)smps;
    sample_decref(*smp);
  });

  m.def("sample_length",
        [](void *smp) -> unsigned { return sample_length((vsample *)smp); });

  m.def("sample_pack", &sample_pack, py::return_value_policy::reference);

  m.def(
      "sample_unpack",
      [](void *smp, unsigned *seq, struct timespec *ts_origin,
         struct timespec *ts_received, int *flags, unsigned *len,
         double *values) -> void {
        return sample_unpack((vsample *)smp, seq, ts_origin, ts_received, flags,
                             len, values);
      },
      py::return_value_policy::reference);

  py::class_<Array>(m, "SamplesArray")
      .def(py::init<unsigned int>(), py::arg("len"))
      .def("__getitem__",
           [](Array &a, unsigned int idx) {
             if (idx >= a.size()) {
               throw py::index_error("Index out of bounds");
             }
             return a[idx];
           })
      .def("__setitem__", [](Array &a, unsigned int idx, void *smp) {
        if (idx >= a.size()) {
          throw py::index_error("Index out of bounds");
        }
        if (a[idx]) {
          sample_decref(a[idx]);
        }
        a[idx] = (vsample *)smp;
      });
}
