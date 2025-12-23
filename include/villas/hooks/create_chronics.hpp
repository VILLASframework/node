/* Create Chronics hook.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hook.hpp>
#include <villas/sample.hpp>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace villas {
namespace node {

struct GridMapping {
  std::unordered_map<int, int> load_bus;
  std::unordered_map<int, int> sgen_bus;
};

struct ChronicsOptions {
  std::filesystem::path loads_dir;
  std::filesystem::path sgens_dir;
  std::filesystem::path grid_path;
  std::filesystem::path output_dir;
  int round_decimals =3;
  bool compress = true;
  bool negate_sgens = true;
  float voltage = 20.0;

  static ChronicsOptions from_json(const json_t &cfg);
};

class ChronicsHook : public Hook {

  public:
    using Hook::Hook;

    void run();
    void flush();
    GridMapping load_grid();
    void discover_files();
    void process_load_files();
    void process_sgen_files();
    void process_samples_from_file(const std::string &filename, const std::vector<Sample *> &samples);
    void round_values();
    void write_outputs();
    void discover_files_from_node(struct file *f);

    void prepare() override;
    void start() override;
    void stop() override;


  private:
    ChronicsOptions options;
    GridMapping mapping;
    std::vector<std::filesystem::path> load_files;
    std::vector<std::filesystem::path> sgen_files;
    std::vector<std::vector<double>> load_p_columns;
    std::vector<std::vector<double>> load_q_columns;
    std::vector<std::vector<double>> prod_p_columns;
    std::vector<std::vector<double>> prod_q_columns;
    std::vector<std::vector<double>> prod_v_columns;
    std::vector<std::string> column_names;
    std::string load_col_names;
    std::string sgen_col_names;
    bool done;
    unsigned sgen_idx;
};

}
}
