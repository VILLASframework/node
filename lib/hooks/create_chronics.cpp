/* Create Chronics hook.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <bzlib.h>
#include <jansson.h>
#include <nlohmann/json.hpp>
#include <openssl/ec.h>

#include <villas/hooks/create_chronics.hpp>
#include <villas/sample.hpp>

#include "villas/node_compat.hpp"
#include "villas/nodes/file.hpp"

using namespace villas;
using namespace villas::node;

static int extract_file_number(const std::filesystem::path &file) {
  static const std::regex re("(\\d+)");
  std::smatch match;

  std::string name = file.stem().string();

  if (!std::regex_search(name, match, re))
    throw std::runtime_error("No numeric index in filename: " + file.string());

  return std::stoi(match.str(1));
}

static double round_dec(double value, unsigned decimals) {
  double scale =
      std::round(value * static_cast<double>(std::pow(10, decimals))) /
      std::pow(10, decimals);
  return scale;
}

static void parse_table(const nlohmann::json &table_df,
                        std::unordered_map<int, int> &target,
                        const std::string col) {
  const nlohmann::json &load = table_df.at("_object").at(col);
  std::string s = table_df["_object"][col]["_object"].get<std::string>();

  std::string load_obj_str = load.at("_object").get<std::string>();
  nlohmann::json load_df = nlohmann::json::parse(load_obj_str);

  const auto &cols = load_df.at("columns");
  auto it = std::find(cols.begin(), cols.end(), "bus");
  if (it == cols.end()) {
    throw std::runtime_error("load dataframe does not contain a 'bus' column");
  }
  size_t bus_col = static_cast<size_t>(std::distance(cols.begin(), it));

  const auto &idxs = load_df.at("index");
  const auto &rows = load_df.at("data");

  if (idxs.size() != rows.size()) {
    throw std::runtime_error("'index' and 'data' length mismatch");
  }

  for (size_t i = 0; i < rows.size(); ++i) {
    int idx = idxs.at(i).get<int>();
    const auto &row = rows.at(i);
    const auto &bus_cell = row.at(bus_col);

    int bus = bus_cell.get<int>();
    target[idx] = bus;
  }
}

static std::vector<std::filesystem::path>
glob_sorted(const std::filesystem::path &dir, const std::string &prefix) {
  std::vector<std::filesystem::path> files;
  if (!std::filesystem::exists(dir))
    throw std::runtime_error("Directory missing: " + dir.string());

  for (auto &entry : std::filesystem::directory_iterator(dir)) {
    if (!entry.is_regular_file())
      continue;
    auto name = entry.path().filename().string();
    if (name.rfind(prefix, 0) == 0 && entry.path().extension() == ".csv")
      files.push_back(entry.path());
  }

  std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
    return extract_file_number(a) < extract_file_number(b);
  });

  return files;
}

//Create Chronics members
ChronicsOptions ChronicsOptions::from_json(const json_t &config) {

  char *json_string = json_dumps(&config, JSON_COMPACT);
  nlohmann::json cfg = nlohmann::json::parse(json_string);

  ChronicsOptions opts;

  if (!cfg.contains("loads_dir") || !cfg.contains("sgens_dir") ||
      !cfg.contains("grid") || !cfg.contains("output"))
    throw std::runtime_error(
        "chronics: loads_dir, sgens_dir, grid, output are required");

  opts.loads_dir = cfg.at("loads_dir").get<std::string>();
  opts.sgens_dir = cfg.at("sgens_dir").get<std::string>();
  opts.grid_path = cfg.at("grid").get<std::string>();
  opts.output_dir = cfg.at("output").get<std::string>();

  if (cfg.contains("round_decimals"))
    opts.round_decimals = cfg.at("round_decimals").get<unsigned>();

  if (cfg.contains("compress"))
    opts.compress = cfg.at("compress").get<bool>();

  if (cfg.contains("voltage"))
    opts.voltage = cfg.at("voltage").get<float>();

  return opts;
}

GridMapping ChronicsHook::load_grid() {
  std::ifstream in(options.grid_path);

  if (!in)
    throw std::runtime_error("Cannot open grid file: " +
                             options.grid_path.string());

  nlohmann::json grid_json;
  in >> grid_json;

  GridMapping mapping;

  parse_table(grid_json, mapping.load_bus, "load");
  parse_table(grid_json, mapping.sgen_bus, "sgen");

  return mapping;
}

void ChronicsHook::discover_files() {

  load_files = glob_sorted(options.loads_dir, "Load");
  sgen_files = glob_sorted(options.sgens_dir, "SGen");

  if (load_files.empty() && sgen_files.empty())
    throw std::runtime_error("chronics_hook: no csv files found");
}

void ChronicsHook::discover_files_from_node(struct file *f) {
  load_files.clear();
  sgen_files.clear();

  for (const auto &[filename, samples] : f->file_samples) {
    std::filesystem::path path(filename);
    std::string name = path.filename().string();
  }

  load_files = glob_sorted(options.loads_dir, "Load");
  sgen_files = glob_sorted(options.sgens_dir, "SGen");

  std::sort(load_files.begin(), load_files.end(),
            [](const auto &a, const auto &b) {
              return extract_file_number(a) < extract_file_number(b);
            });
  std::sort(sgen_files.begin(), sgen_files.end(),
            [](const auto &a, const auto &b) {
              return extract_file_number(a) < extract_file_number(b);
            });
}

void ChronicsHook::process_load_files() {

  auto *file_node = dynamic_cast<NodeCompat *>(node);
  if (file_node) {
    auto *f = file_node->getData<struct file>();

    if (f->multi_file_mode) {
      // Process files from file node
      for (const auto &file_path : load_files) {
        std::string filename = file_path.string();
        auto it = f->file_samples.find(filename);
        if (it != f->file_samples.end())
          process_samples_from_file(filename, it->second);
      }
    } else
      std::runtime_error("Multi file mode is not active");
  }
}

void ChronicsHook::process_sgen_files() {
  auto *file_node = dynamic_cast<NodeCompat *>(node);
  if (file_node) {
    auto *f = file_node->getData<struct file>();

    if (f->multi_file_mode) {
      // Process files from file node
      for (const auto &file_path : sgen_files) {
        std::string filename = file_path.string();
        auto it = f->file_samples.find(filename);
        if (it != f->file_samples.end()) {
          process_samples_from_file(filename, it->second);
        }
      }
    } else
      std::runtime_error("Multi file mode is not active");
  }
}

void ChronicsHook::process_samples_from_file(
    const std::string &filename, const std::vector<Sample *> &samples) {

  if (samples.empty()) {
    logger->warn("No samples read from file: {}", filename);
    return;
  }

  auto *first_sample = samples[0];
  if (!first_sample->signals) {
    throw std::runtime_error("Sample has no signal list: " + filename);
  }

  // auto p_norm_sig = first_sample->signals->getByName("P_norm");
  auto p_norm_sig = first_sample->signals->getByName("signal0");
  auto q_norm_sig = first_sample->signals->getByName("signal1");

  if (!p_norm_sig || !q_norm_sig)
    throw std::runtime_error("File missing P_norm or Q_norm signals: " +
                             filename);

  size_t p_norm_idx = first_sample->signals->getIndexByName("signal0");
  size_t q_norm_idx = first_sample->signals->getIndexByName("signal1");

  std::vector<double> p_norm,
      q_norm; /* , prod_p_norm, prod_q_norm, prod_v_norm; */
  for (auto *smp : samples) {
    if (smp->length > p_norm_idx && smp->length > q_norm_idx) {
      p_norm.push_back(smp->data[p_norm_idx].f);
      q_norm.push_back(smp->data[q_norm_idx].f);
    }
  }

  std::filesystem::path path(filename);
  int element_idx = extract_file_number(path);
  std::string::size_type is_load =
      path.filename().string().rfind("Load", 0 == 0);

  int bus_id;
  if (is_load != std::string::npos) {
    auto it = mapping.load_bus.find(element_idx);
    if (it == mapping.load_bus.end())
      throw std::runtime_error("Load index missing in grid mapping: " +
                               std::to_string(element_idx));

    bus_id = it->second;

    load_p_columns.push_back(std::move(p_norm));
    load_q_columns.push_back(std::move(q_norm));
    // Set column names for load files
    int col_idx = static_cast<int>(load_p_columns.size());
    std::string col_name =
        "load_" + std::to_string(bus_id) + "_" + std::to_string(col_idx - 1);
    // TODO: Logic to write the column names to the column name vector
    if (!load_col_names.empty())
      load_col_names += ';';
    load_col_names += col_name;

  } else {

    auto it = mapping.sgen_bus.find(element_idx);
    if (it == mapping.sgen_bus.end())
      throw std::runtime_error("SGen index missing in grid mapping: " +
                               std::to_string(element_idx));

    bus_id = it->second;
    prod_p_columns.push_back(std::move(p_norm));
    prod_q_columns.push_back(std::move(q_norm));

    // Create a vector for prod_v
    std::vector<double> prod_v;
    for (size_t i = 0; i < prod_p_columns[0].size(); i++) {
      // prod_v[i] = options.voltage;
      prod_v.push_back(options.voltage);
    }
    prod_v_columns.push_back(prod_v);

    std::string col_name =
        "sgen_" + std::to_string(bus_id) + "_" + std::to_string(sgen_idx);
    if (!sgen_col_names.empty())
      sgen_col_names += ';';
    sgen_col_names += col_name;
    sgen_idx += 1;
  }
}

void ChronicsHook::round_values() {

  for (auto &col : load_p_columns)
    for (double &v : col)
      v = round_dec(v, options.round_decimals);

  for (auto &col : load_q_columns)
    for (double &v : col)
      v = round_dec(v, options.round_decimals);
}

static void write_csv(const std::filesystem::path &path,
                      const std::string &header,
                      const std::vector<std::vector<double>> &columns) {
  std::ofstream out(path);
  if (!out)
    throw std::runtime_error("Cannot open output file: " + path.string());

  out << header;
  out << '\n';

  size_t rows = columns.empty() ? 0 : columns.front().size();
  for (size_t r = 0; r < rows; r++) {
    for (size_t c = 0; c < columns.size(); c++) {
      if (c)
        out << ';';
      out << columns[c][r];
    }
    out << '\n';
  }
}

static void bz2_write_all(BZFILE *bzf, const std::string &s) {
  int err = BZ_OK;
  BZ2_bzWrite(&err, bzf, const_cast<char *>(s.data()),
              static_cast<int>(s.size()));
  if (err != BZ_OK)
    throw std::runtime_error("BZ2_bzWrite failed");
}

static void write_bz2(const std::filesystem::path &path,
                      const std::string &header,
                      const std::vector<std::vector<double>> &columns) {

  FILE *fp = std::fopen(path.string().c_str(), "wb");
  if (!fp)
    throw std::runtime_error("Cannot open output file: " + path.string());

  int err = BZ_OK;
  BZFILE *bzf = BZ2_bzWriteOpen(&err, fp, 9, 0, 30);
  if (!bzf || err != BZ_OK) {
    std::fclose(fp);
    throw std::runtime_error("BZ2_bzWriteOpen failed");
  }

  try {
    bz2_write_all(bzf, header);
    bz2_write_all(bzf, "\n");

    const size_t rows = columns.empty() ? 0 : columns.front().size();
    for (size_t r = 0; r < rows; ++r) {
      std::ostringstream line;
      for (size_t c = 0; c < columns.size(); ++c) {
        if (c)
          line << ';';
        line << columns[c][r];
      }
      line << '\n';
      bz2_write_all(bzf, line.str());
    }
    unsigned int nbytes_in = 0, nbytes_out = 0;
    BZ2_bzWriteClose(&err, bzf, /*abandon*/ 0, &nbytes_in, &nbytes_out);
    bzf = nullptr; // safety
    std::fclose(fp);
    if (err != BZ_OK)
      throw std::runtime_error("BZ2_bzWriteClose failed");
  } catch (...) {
    int dummy = BZ_OK;
    if (bzf)
      BZ2_bzWriteClose(&dummy, bzf, /*abandon*/ 1, nullptr, nullptr);
    std::fclose(fp);
    throw;
  }
}

void ChronicsHook::write_outputs() {
  std::filesystem::create_directories(options.output_dir);
  if (!options.compress) {
    auto load_p_path = options.output_dir / "load_p.csv";
    auto load_q_path = options.output_dir / "load_q.csv";
    auto prod_p_path = options.output_dir / "prod_p.csv";
    auto prod_q_path = options.output_dir / "prod_q.csv";
    auto prod_v_path = options.output_dir / "prod_v.csv";
    write_csv(load_p_path, load_col_names, load_p_columns);
    write_csv(load_q_path, load_col_names, load_q_columns);
    write_csv(prod_p_path, sgen_col_names, prod_p_columns);
    write_csv(prod_q_path, sgen_col_names, prod_q_columns);
    write_csv(prod_v_path, sgen_col_names, prod_v_columns);
  } else {
    auto load_p_bzip2_path = options.output_dir / "load_p.csv.bz2";
    auto load_q_bzip2_path = options.output_dir / "load_q.csv.bz2";
    auto prod_p_bzip2_path = options.output_dir / "prod_p.csv.bz2";
    auto prod_q_bzip2_path = options.output_dir / "prod_q.csv.bz2";
    auto prod_v_bzip2_path = options.output_dir / "prod_v.csv.bz2";
    write_bz2(load_p_bzip2_path, load_col_names, load_p_columns);
    write_bz2(load_q_bzip2_path, load_col_names, load_q_columns);
    write_bz2(prod_p_bzip2_path, sgen_col_names, prod_q_columns);
    write_bz2(prod_q_bzip2_path, sgen_col_names, prod_q_columns);
    write_bz2(prod_v_bzip2_path, sgen_col_names, prod_v_columns);
  }
}

void ChronicsHook::run() {
  process_load_files();
  process_sgen_files();
  round_values();
  write_outputs();
  done = 1;
}

void ChronicsHook::flush() {}

void ChronicsHook::prepare() {

  try {
    sgen_idx = 0;
    options = ChronicsOptions::from_json(*config);
    mapping = load_grid();
  } catch (const std::exception &ex) {
    logger->error("chronics prepare failed: {}", ex.what());
  }
}

void ChronicsHook::start() {
  auto *file_node = dynamic_cast<NodeCompat *>(node);
  if (file_node) {
    auto *f = file_node->getData<struct file>();

    if (f->multi_file_mode) {
      discover_files_from_node(f);
      run();
    }
  }
}

void ChronicsHook::stop() {
  if (done)
    state = State::STOPPED;
  return Hook::stop();
}

static char n[] = "create_chronics";
static char d[] = "This hook creates chronics from csv files";
static HookPlugin<ChronicsHook, n, d,
                  (int)Hook::Flags::NODE_READ | (int)Hook::Flags::NODE_WRITE |
                      (int)Hook::Flags::PATH>
    p;
