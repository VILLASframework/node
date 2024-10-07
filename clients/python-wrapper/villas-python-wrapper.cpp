#include <jansson.h>
#include <pybind11/pybind11.h>
#include <uuid/uuid.h>
#include <villas/node.h>
#include <villas/node.hpp>
#include <villas/sample.hpp>

namespace py = pybind11;

//pybind11 can not deal with (void **) as function input parameters,
//therefore we have to cast a simple (void *) pointer to the corresponding type
//
//thus:
//convenience functions for dealing with:
//
//(vnode *)
villas::node::Node *node_cast(void *p){
  vnode *n = static_cast<vnode *>(p);
  auto *nc = reinterpret_cast<villas::node::Node *>(n);
  return nc;
}
//
//(vsample *)
villas::node::Sample *sample_cast(void *p){
  vsample *smp = static_cast<vsample *>(p);
  auto *smpc = reinterpret_cast<villas::node::Sample *>(smp);
  return smpc;
}
//
//(vsample **)
villas::node::Sample **samples_cast(void *pp){
  vsample **smps = static_cast<vsample **>(pp);
  auto **smpsc = reinterpret_cast<villas::node::Sample **>(smps);
  return smpsc;
}

//wrapper bindings, sorted alphabetically
// @param villas_node   Name of the module to be bound
// @param m             Access variable for modifying the module code
//
PYBIND11_MODULE(villas-python-wrapper, m) {
  m.def("memory_init", [](int hugepages) -> int {
        return villas::node::memory::init(hugepages);
      });

  m.def("node_check", [](void *p) -> int {
        return node_cast(p)->check();
      });

  m.def("node_destroy", [](void *p) -> int {
        node_cast(p)->~Node();
        return 0;
      });

  m.def("node_details", [](void *p) -> const char * {
        return node_cast(p)->getDetails().c_str();
      },
      py::return_value_policy::copy);

  m.def("node_input_signals_max_cnt", [](void *p) -> unsigned {
        return node_cast(p)->getInputSignalsMaxCount();
      });

  m.def("node_is_enabled", [](void *p) -> int {
        return node_cast(p)->isEnabled();
      });

  m.def("node_is_valid_name", [](const char *name) -> bool {
        return villas::node::Node::isValidName(name);
      });

  m.def("node_name", [](void *p) -> const char * {
        return node_cast(p)->getName().c_str();
      },
      py::return_value_policy::copy);

  m.def("node_name_full", [](void *p) -> const char * {
        return node_cast(p)->getNameFull().c_str();
      },
      py::return_value_policy::copy);

  m.def("node_name_short", [](void *p) -> const char * {
        return node_cast(p)->getNameShort().c_str();
      },
      py::return_value_policy::copy);

  m.def("node_netem_fds", [](void *p, int fds[]) -> int {
        villas::node::Node *nc = node_cast(p);
        auto l = nc->getNetemFDs();

        for (unsigned i = 0; i < l.size() && i < 16; i++){
          fds[i] = l[i];
        }

        return l.size();
      });

  m.def("node_new", [](const char *id_str, const char *json_str) -> vnode * {
        json_error_t err;
        uuid_t id;
        uuid_parse(id_str, id);
        auto *json = json_loads(json_str, 0, &err);
        return (vnode *)villas::node::NodeFactory::make(json, id);
      },
      py::return_value_policy::reference);

  m.def("node_output_signals_max_cnt", [](void *p) -> unsigned {
        return node_cast(p)->getOutputSignalsMaxCount();
      });

  m.def("node_pause", [](void *p) -> int {
        return node_cast(p)->pause();
      });

  m.def("node_poll_fds", [](void *p, int fds[]) -> int {
        villas::node::Node *nc = node_cast(p);
        auto l = nc->getPollFDs();

        for (unsigned i = 0; i < l.size() && i < 16; i++){
          fds[i] = l[i];
        }

        return l.size();
      });

  m.def("node_prepare", [](void *p) -> int {
        return node_cast(p)->prepare();
      });

  m.def("node_read", [](void *p, void *pp, unsigned cnt) -> int {
        return node_cast(p)->read(samples_cast(pp), cnt);
      });

  m.def("node_restart", [](void *p) -> int {
        return node_cast(p)->restart();
      });

  m.def("node_resume", [](void *p) -> int {
        return node_cast(p)->resume();
      });

  m.def("node_reverse", [](void *p) -> int {
        return node_cast(p)->reverse();
      });

  m.def("node_start", [](void *p) -> int {
        return node_cast(p)->start();
      });

  m.def("node_stop", [](void *p) -> int {
        return node_cast(p)->stop();
      });

  m.def("node_to_json", [](void *p) -> json_t * {
        return node_cast(p)->toJson();
      },
      py::return_value_policy::copy);

  m.def("node_to_json_str", [](void *p) -> const char * {
        auto json = node_cast(p)->toJson();
        return json_dumps(json, 0);
      },
      py::return_value_policy::copy);

  m.def("node_write", [](void *p, void *pp, unsigned cnt) -> int {
        return node_cast(p)->write(samples_cast(pp), cnt);
      });

  m.def("sample_alloc", [](unsigned len) -> vsample * {
        return (vsample *)villas::node::sample_alloc_mem(len);
      }, py::return_value_policy::reference);

  // Decrease reference count and release memory if last reference was held.
  m.def("sample_decref", [](void *p) -> void {
        villas::node::sample_decref(sample_cast(p));
      });

  m.def("sample_length", [](void *p) -> unsigned {
        return sample_cast(p)->length;
      });

  m.def("sample_pack", [](unsigned seq, struct timespec *ts_origin,
                          struct timespec *ts_received, unsigned len,
                          double *values) -> vsample * {
        auto *smp = villas::node::sample_alloc_mem(len);

        smp->sequence = seq;
        smp->ts.origin = *ts_origin;
        smp->ts.received = *ts_received;
        smp->length = len;
        smp->flags = (int)villas::node::SampleFlags::HAS_SEQUENCE | (int)villas::node::SampleFlags::HAS_DATA |
                    (int)villas::node::SampleFlags::HAS_TS_ORIGIN;

        memcpy((double *)smp->data, values, sizeof(double) * len);

        return (vsample *)smp;
      }, py::return_value_policy::reference);

  m.def("sample_unpack", [](void *p, unsigned *seq, struct timespec *ts_origin,
                            struct timespec *ts_received, int *flags, unsigned *len,
                            double *values) -> void {
        auto *smp = sample_cast(p);

        *seq = smp->sequence;

        *ts_origin = smp->ts.origin;
        *ts_received = smp->ts.received;

        *flags = smp->flags;
        *len = smp->length;

        memcpy(values, (double *)smp->data, sizeof(double) * *len);
      },
      py::return_value_policy::reference);
}

