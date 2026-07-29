/* Create chronics files for OpenDSS.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <bzlib.h>
#include <nlohmann/json.hpp>
#include <unistd.h>

#include <villas/log.hpp>
#include <villas/tool.hpp>

namespace villas {
namespace node {
namespace tools {

static int extract_file_number(const std::filesystem::path &file) {
  static const std::regex re("(\\d+)");
  std::smatch match;

  const std::string name = file.stem().string();
  if (!std::regex_search(name, match, re)) {
    throw std::runtime_error("No numeric index in filename: " + file.string());
  }
  return std::stoi(match.str(1));
}

static double round_dec(double value, unsigned decimals) {
  return std::round(value * static_cast<double>(std::pow(10, decimals))) /
         std::pow(10, decimals);
}

static void parse_table(const nlohmann::json &table_df,
                        std::unordered_map<int, int> &target,
                        const std::string &col) {
  const nlohmann::json &load = table_df.at("_object").at(col);
  const std::string load_obj_str = load.at("_object").get<std::string>();
  nlohmann::json load_df = nlohmann::json::parse(load_obj_str);

  const auto &cols = load_df.at("columns");
  auto it = std::find(cols.begin(), cols.end(), "bus");
  if (it == cols.end()) {
    throw std::runtime_error("load dataframe does not contain a 'bus' column");
  }
  const size_t bus_col = static_cast<size_t>(std::distance(cols.begin(), it));

  const auto &idxs = load_df.at("index");
  const auto &rows = load_df.at("data");
  if (idxs.size() != rows.size()) {
    throw std::runtime_error("'index' and 'data' length mismatch");
  }

  for (size_t i = 0; i < rows.size(); ++i) {
    const int idx = idxs.at(i).get<int>();
    const auto &row = rows.at(i);
    const auto &bus_cell = row.at(bus_col);
    const int bus = bus_cell.get<int>();
    target[idx] = bus;
  }
}

static std::vector<std::filesystem::path>
glob_sorted(const std::filesystem::path &dir, const std::string &prefix) {
  std::vector<std::filesystem::path> files;
  if (!std::filesystem::exists(dir)) {
    throw std::runtime_error("Directory missing: " + dir.string());
  }

  for (auto &entry : std::filesystem::directory_iterator(dir)) {
    if (!entry.is_regular_file())
      continue;
    const auto name = entry.path().filename().string();
    if (name.rfind(prefix, 0) == 0 && entry.path().extension() == ".csv")
      files.push_back(entry.path());
  }

  std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
    return extract_file_number(a) < extract_file_number(b);
  });
  return files;
}

static size_t find_column_index(const std::vector<std::string> &columns,
                                const std::string &name,
                                const std::filesystem::path &path) {
  auto it = std::find(columns.begin(), columns.end(), name);
  if (it == columns.end())
    throw std::runtime_error("Column " + name + " missing in " + path.string());
  return static_cast<size_t>(std::distance(columns.begin(), it));
}

static std::pair<std::vector<double>, std::vector<double>>
load_pq_series_csv(const std::filesystem::path &path) {
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error("Cannot open CSV: " + path.string());

  std::string header;
  if (!std::getline(in, header))
    throw std::runtime_error("Empty CSV: " + path.string());

  std::vector<std::string> columns;
  {
    std::stringstream ss(header);
    std::string token;
    while (std::getline(ss, token, ',')) {
      columns.push_back(token);
    }
  }

  const size_t p_idx = find_column_index(columns, "P_norm", path);
  const size_t q_idx = find_column_index(columns, "Q_norm", path);

  std::vector<double> p_norm;
  std::vector<double> q_norm;

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty())
      continue;

    std::stringstream ss(line);
    std::string cell;
    size_t col = 0;
    double p = 0.0;
    double q = 0.0;

    while (std::getline(ss, cell, ',')) {
      if (col == p_idx)
        p = std::stod(cell);
      else if (col == q_idx)
        q = std::stod(cell);
      ++col;
    }

    p_norm.push_back(p);
    q_norm.push_back(q);
  }

  return {std::move(p_norm), std::move(q_norm)};
}

static void write_csv(const std::filesystem::path &path,
                      const std::string &header,
                      const std::vector<std::vector<double>> &columns) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("Cannot open output file: " + path.string());
  }

  out << header << '\n';

  const size_t rows = columns.empty() ? 0 : columns.front().size();
  for (size_t r = 0; r < rows; ++r) {
    for (size_t c = 0; c < columns.size(); ++c) {
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
    BZ2_bzWriteClose(&err, bzf, 0, &nbytes_in, &nbytes_out);
    bzf = nullptr;
    std::fclose(fp);
    if (err != BZ_OK)
      throw std::runtime_error("BZ2_bzWriteClose failed");
  } catch (...) {
    int dummy = BZ_OK;
    if (bzf)
      BZ2_bzWriteClose(&dummy, bzf, 1, nullptr, nullptr);
    std::fclose(fp);
    throw;
  }
}

class CreateChronics : public Tool {
public:
  CreateChronics(int argc, char *argv[]) : Tool(argc, argv, "chronics") {}

protected:
  std::filesystem::path config_path;

  void usage() override {
    std::cout << "Usage: villas-chronics [OPTIONS] CONFIG.json" << std::endl
              << std::endl
              << "OPTIONS:" << std::endl
              << "  -d LVL  set debug log level" << std::endl
              << "  -h      show this help" << std::endl
              << "  -V      show version and exit" << std::endl
              << std::endl;
    printCopyright();
  }

  void parse() override {
    int c;
    while ((c = getopt(argc, argv, "d:Vh")) != -1) {
      switch (c) {
      case 'd':
        Log::getInstance().setLevel(optarg);
        break;
      case 'V':
        printVersion();
        exit(EXIT_SUCCESS);
      case 'h':
      case '?':
        usage();
        exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
      }
    }

    if (argc - optind < 1) {
      usage();
      exit(EXIT_FAILURE);
    }

    config_path = argv[optind];
  }

  int main() override {
    nlohmann::json cfg;
    {
      std::ifstream in(config_path);
      if (!in)
        throw std::runtime_error("Cannot open config file: " +
                                 config_path.string());
      in >> cfg;
    }

    options = ChronicsOptions::from_json(cfg);
    sgen_idx = 0;

    mapping = load_grid();
    discover_files();
    process_load_files();
    process_sgen_files();
    round_values();
    write_outputs();

    return 0;
  }

private:
  struct GridMapping {
    std::unordered_map<int, int> load_bus;
    std::unordered_map<int, int> sgen_bus;
  };

  struct ChronicsOptions {
    std::filesystem::path loads_dir;
    std::filesystem::path sgens_dir;
    std::filesystem::path grid_path;
    std::filesystem::path output_dir;

    unsigned round_decimals = 3;
    bool compress = true;
    bool negate_sgens = true;

    float voltage = 20.0;

    static ChronicsOptions from_json(const nlohmann::json &cfg) {
      if (!cfg.contains("loads_dir") || !cfg.contains("sgens_dir") ||
          !cfg.contains("grid") || !cfg.contains("output")) {
        throw std::runtime_error(
            "chronics: loads_dir, sgens_dir, grid, output are required");
      }

      ChronicsOptions opts;
      opts.loads_dir = cfg.at("loads_dir").get<std::string>();
      opts.sgens_dir = cfg.at("sgens_dir").get<std::string>();
      opts.grid_path = cfg.at("grid").get<std::string>();
      opts.output_dir = cfg.at("output").get<std::string>();

      if (cfg.contains("round_decimals"))
        opts.round_decimals = cfg.at("round_decimals").get<unsigned>();
      if (cfg.contains("compress"))
        opts.compress = cfg.at("compress").get<bool>();
      if (cfg.contains("negate_sgens"))
        opts.negate_sgens = cfg.at("negate_sgens").get<bool>();
      if (cfg.contains("voltage"))
        opts.voltage = cfg.at("voltage").get<float>();

      return opts;
    }
  };

  ChronicsOptions options;
  GridMapping mapping;

  std::vector<std::filesystem::path> load_files;
  std::vector<std::filesystem::path> sgen_files;

  std::vector<std::vector<double>> load_p_columns;
  std::vector<std::vector<double>> load_q_columns;

  std::vector<std::vector<double>> prod_p_columns;
  std::vector<std::vector<double>> prod_q_columns;
  std::vector<std::vector<double>> prod_v_columns;

  std::string load_col_names;
  std::string sgen_col_names;
  unsigned sgen_idx = 0;

  GridMapping load_grid() {
    std::ifstream in(options.grid_path);
    if (!in)
      throw std::runtime_error("Cannot open grid file: " +
                               options.grid_path.string());

    nlohmann::json grid_json;
    in >> grid_json;

    GridMapping m;
    parse_table(grid_json, m.load_bus, "load");
    parse_table(grid_json, m.sgen_bus, "sgen");
    return m;
  }

  void discover_files() {
    load_files = glob_sorted(options.loads_dir, "Load");
    sgen_files = glob_sorted(options.sgens_dir, "SGen");

    if (load_files.empty() && sgen_files.empty()) {
      throw std::runtime_error("chronics_hook: no csv files found");
    }
  }

  void process_load_files() {
    for (const auto &file : load_files) {
      const int element_index = extract_file_number(file);
      auto it = mapping.load_bus.find(element_index);
      if (it == mapping.load_bus.end())
        throw std::runtime_error("Load index missing in grid mapping: " +
                                 std::to_string(element_index));

      const auto [p_norm, q_norm] = load_pq_series_csv(file);
      load_p_columns.push_back(p_norm);
      load_q_columns.push_back(q_norm);

      const int bus_id = it->second;
      const size_t col_idx = load_p_columns.size() - 1;
      const std::string col_name =
          "load_" + std::to_string(bus_id) + "_" + std::to_string(col_idx);

      if (!load_col_names.empty())
        load_col_names += ';';
      load_col_names += col_name;
    }
  }

  void process_sgen_files() {
    for (const auto &file : sgen_files) {
      const int element_index = extract_file_number(file);
      auto it = mapping.sgen_bus.find(element_index);
      if (it == mapping.sgen_bus.end())
        throw std::runtime_error("SGen index missing in grid mapping: " +
                                 std::to_string(element_index));

      const auto [p_norm, q_norm] = load_pq_series_csv(file);
      const int bus_id = it->second;

      prod_p_columns.push_back(p_norm);
      prod_q_columns.push_back(q_norm);
      prod_v_columns.push_back(
          std::vector<double>(p_norm.size(), options.voltage));

      const std::string col_name =
          "sgen_" + std::to_string(bus_id) + "_" + std::to_string(sgen_idx);
      if (!sgen_col_names.empty())
        sgen_col_names += ';';
      sgen_col_names += col_name;

      ++sgen_idx;
    }
  }

  void round_values() {
    for (auto &col : load_p_columns)
      for (double &v : col)
        v = round_dec(v, options.round_decimals);

    for (auto &col : load_q_columns)
      for (double &v : col)
        v = round_dec(v, options.round_decimals);
  }

  void write_outputs() {
    std::filesystem::create_directories(options.output_dir);

    if (!options.compress) {
      write_csv(options.output_dir / "load_p.csv", load_col_names,
                load_p_columns);
      write_csv(options.output_dir / "load_q.csv", load_col_names,
                load_q_columns);

      write_csv(options.output_dir / "prod_p.csv", sgen_col_names,
                prod_p_columns);
      write_csv(options.output_dir / "prod_q.csv", sgen_col_names,
                prod_q_columns);
      write_csv(options.output_dir / "prod_v.csv", sgen_col_names,
                prod_v_columns);
    } else {
      write_bz2(options.output_dir / "load_p.csv.bz2", load_col_names,
                load_p_columns);
      write_bz2(options.output_dir / "load_q.csv.bz2", load_col_names,
                load_q_columns);

      write_bz2(options.output_dir / "prod_p.csv.bz2", sgen_col_names,
                prod_p_columns);
      write_bz2(options.output_dir / "prod_q.csv.bz2", sgen_col_names,
                prod_q_columns);
      write_bz2(options.output_dir / "prod_v.csv.bz2", sgen_col_names,
                prod_v_columns);
    }
  }
};

} // namespace tools
} // namespace node
} // namespace villas

int main(int argc, char *argv[]) {
  return villas::node::tools::CreateChronics(argc, argv).run();
}
