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
  int norm, flex, fixed, total;

  width = w;

  norm = 0;
  flex = 0;
  fixed = 0;
  total = width - columns.size() * 2;

  // Normalize width
  for (unsigned i = 0; i < columns.size(); i++) {
    if (columns[i].width > 0)
      norm += columns[i].width;
    if (columns[i].width == 0)
      flex++;
    if (columns[i].width < 0)
      fixed += -1 * columns[i].width;
  }

  for (unsigned i = 0; i < columns.size(); i++) {
    if (columns[i].width > 0)
      columns[i]._width = columns[i].width * (float)(total - fixed) / norm;
    if (columns[i].width == 0)
      columns[i]._width = (float)(total - fixed) / flex;
    if (columns[i].width < 0)
      columns[i]._width = -1 * columns[i].width;
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
  if (width != Log::getInstance().getWidth())
    resize(Log::getInstance().getWidth());

  char *line1 = nullptr;
  char *line2 = nullptr;
  char *line3 = nullptr;

  for (unsigned i = 0; i < columns.size(); i++) {
    int w, u;
    char *col, *unit;

    col = strf(CLR_BLD("%s"), columns[i].title.c_str());
    unit = columns[i].unit.size() ? strf(CLR_YEL("%s"), columns[i].unit.c_str())
                                  : strf("");

    w = columns[i]._width + strlen(col) - strlenp(col);
    u = columns[i]._width + strlen(unit) - strlenp(unit);

    if (columns[i].align == TableColumn::Alignment::LEFT) {
      strcatf(&line1, " %-*.*s" ANSI_RESET, w, w, col);
      strcatf(&line2, " %-*.*s" ANSI_RESET, u, u, unit);
    } else {
      strcatf(&line1, " %*.*s" ANSI_RESET, w, w, col);
      strcatf(&line2, " %*.*s" ANSI_RESET, u, u, unit);
    }

    for (int j = 0; j < columns[i]._width + 2; j++) {
      strcatf(&line3, "%s", BOX_LR);
    }

    if (i != columns.size() - 1) {
      strcatf(&line1, " %s", BOX_UD);
      strcatf(&line2, " %s", BOX_UD);
      strcatf(&line3, "%s", BOX_UDLR);
    }

    free(col);
    free(unit);
  }

  logger->info("{}", line1);
  logger->info("{}", line2);
  logger->info("{}", line3);

  free(line1);
  free(line2);
  free(line3);
}

void Table::row(int count, ...) {
  if (width != Log::getInstance().getWidth()) {
    resize(Log::getInstance().getWidth());
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
