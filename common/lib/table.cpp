/* Print fancy tables.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdlib>
#include <cstring>

#include <villas/boxes.hpp>
#include <villas/colors.hpp>
#include <villas/config.hpp>
#include <villas/log.hpp>
#include <villas/table.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::utils;

#if !defined(LOG_COLOR_DISABLE)
#define ANSI_RESET "\e[0m"
#else
#define ANSI_RESET
#endif

int Table::resize(int w) {
  width = w;

  float norm = 0;
  float flex = 0;
  float fixed = 0;
  float total = width - columns.size() * 3; // Column separators and whitespace

  // Normalize width
  for (auto &column: columns) {
    if (column.width > 0) {
      norm += column.width;
    } else if (column.width == 0) {
      flex++;
    } else if (column.width < 0) {
      fixed += -1 * column.width;
    }
  }

  int total_used = 0;

  for (auto &column: columns) {
    if (column.width > 0) {
      column._width = column.width * (total - fixed) / norm;
    } else if (column.width == 0){
      column._width = (total - fixed) / flex;
    } else if (column.width < 0) {
      column._width = -1 * column.width;
    }

    total_used += column._width;
  }

  // Distribute remaining space over flex columns
  for (auto &column: columns) {
    if (total_used >= total) {
      break;
    }

    if (column.width >= 0) {
      column._width++;
      total_used++;
    }
  }

  updateRowFormat();

  return 0;
}

void Table::updateRowFormat() {
  rowFormat.clear();

  for (unsigned i = 0; i < columns.size(); ++i) {
    auto &column = columns[i];

    rowFormat += " %";
    if (column.align == TableColumn::Alignment::LEFT) {
      rowFormat += "-";
    }

    rowFormat += std::to_string(column._width);

    if (column.precision >= 0) {
      rowFormat += ".";
      rowFormat += std::to_string(column.precision);
    } else if (column.format == "s") {
      rowFormat += ".";
      rowFormat += std::to_string(column._width);
    }

    rowFormat += column.format;

    if (i != columns.size() - 1) {
      rowFormat += " " BOX_UD;
    }
  }
}

void Table::header() {
  auto logWidth = Log::getWidth();
  if (width != logWidth) {
    resize(logWidth);
  }

  char *line1 = nullptr;
  char *line2 = nullptr;
  char *line3 = nullptr;

  for (unsigned i = 0; i < columns.size(); i++) {
    auto &column = columns[i];

    if (column.align == TableColumn::Alignment::LEFT) {
      strcatf(&line1, CLR_BLD(" %*.*s") ANSI_RESET, column._width, column._width, column.title.c_str());
      strcatf(&line2, CLR_YEL(" %*.*s") ANSI_RESET, column._width, column._width, column.unit.empty() ? "" : column.unit.c_str());
    } else {
      strcatf(&line1, CLR_BLD(" %-*.*s") ANSI_RESET, column._width, column._width, column.title.c_str());
      strcatf(&line2, CLR_YEL(" %-*.*s") ANSI_RESET, column._width, column._width, column.unit.empty() ? "" : column.unit.c_str());
    }

    for (int j = 0; j < column._width + 2; j++) {
      strcatf(&line3, "%s", BOX_LR);
    }

    if (i != columns.size() - 1) {
      strcatf(&line1, " %s", BOX_UD);
      strcatf(&line2, " %s", BOX_UD);
      strcatf(&line3, "%s", BOX_UDLR);
    }
  }

  logger->info("{}", line1);
  logger->info("{}", line2);
  logger->info("{}", line3);

  free(line1);
  free(line2);
  free(line3);
}

void Table::row(int count, ...) {
  auto logWidth = Log::getWidth();
  if (width != logWidth) {
    resize(logWidth);
    header();
  }

  char *line;

  va_list args;
  va_start(args, count);

  vasprintf(&line, rowFormat.c_str(), args);

  va_end(args);

  logger->info("{}", line);

  free(line);
}
