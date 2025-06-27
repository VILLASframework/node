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

    rowFormat += " {:" ;
    rowFormat += column.align == TableColumn::Alignment::LEFT ? "<" : ">";
    rowFormat += std::to_string(column._width);

    if (column.precision >= 0) {
      rowFormat += fmt::format(".{}", column.precision);
    } else if (column.format == "s") {
      rowFormat += fmt::format(".{}", column._width);
    }

    rowFormat += column.format;
    rowFormat += "}";

    if (i != columns.size() - 1) {
      rowFormat += "} " BOX_UD;
    }
  }
}

void Table::header() {
  auto logWidth = Log::getWidth();
  if (width != logWidth) {
    resize(logWidth);
  }

  std::string line1;
  std::string line2;
  std::string line3;

  for (unsigned i = 0; i < columns.size(); i++) {
    auto &column = columns[i];

    auto leftAligned = column.align == TableColumn::Alignment::LEFT;
    line1 += fmt::format(leftAligned ? CLR_BLD(" {0:<{1}.{1}s}") : CLR_BLD(" {0:>{1}.{1}s}"), column.title, column._width);
    line2 += fmt::format(leftAligned ? CLR_YEL(" {0:<{1}.{1}s}") : CLR_YEL(" {0:>{1}.{1}s}"), column.unit, column._width);

    for (int j = 0; j < column._width + 2; j++) {
      line3 += BOX_LR;
    }

    if (i != columns.size() - 1) {
      line1 += " " BOX_UD;
      line2 += " " BOX_UD;
      line3 += BOX_UDLR;
    }
  }

  logger->info(line1);
  logger->info(line2);
  logger->info(line3);
}
