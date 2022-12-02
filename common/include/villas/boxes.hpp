/** Various helper functions.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

// The suffixed of the BOX_ macro a constructed by
// combining the following letters in the written order:
//   - U for a line facing upwards
//   - D for a line facing downwards
//   - L for a line facing leftwards
//   - R for a line facing rightwards
//
// E.g. a cross can be constructed by combining all line fragments:
//    BOX_UDLR
#if 0 // Alternate character set
  #define BOX(chr)	"\e(0" chr "\e(B"
  #define BOX_LR	BOX("\x71") // Boxdrawing: ─
  #define BOX_UD	BOX("\x78") // Boxdrawing: │
  #define BOX_UDR	BOX("\x74") // Boxdrawing: ├
  #define BOX_UDLR	BOX("\x6E") // Boxdrawing: ┼
  #define BOX_UDL	BOX("\x75") // Boxdrawing: ┤
  #define BOX_ULR	BOX("\x76") // Boxdrawing: ┴
  #define BOX_UL	BOX("\x6A") // Boxdrawing: ┘
  #define BOX_DLR	BOX("\x77") // Boxdrawing: ┘
  #define BOX_DL	BOX("\x6B") // Boxdrawing: ┘
#elif 1 // UTF-8
  #define BOX_LR	"─" // Boxdrawing: ─
  #define BOX_UD	"│" // Boxdrawing: │
  #define BOX_UDR	"├" // Boxdrawing: ├
  #define BOX_UDLR	"┼" // Boxdrawing: ┼
  #define BOX_UDL	"┤" // Boxdrawing: ┤
  #define BOX_ULR	"┴" // Boxdrawing: ┴
  #define BOX_UL	"┘" // Boxdrawing: ┘
  #define BOX_DLR	"┬" // Boxdrawing: ┘
  #define BOX_DL	"┐" // Boxdrawing: ┘
  #define BOX_UR	"└" // Boxdrawing: └
#else // ASCII
  #define BOX_LR	"-" // Boxdrawing: ─
  #define BOX_UD	"|" // Boxdrawing: │
  #define BOX_UDR	"+" // Boxdrawing: ├
  #define BOX_UDLR	"+" // Boxdrawing: ┼
  #define BOX_UDL	"+" // Boxdrawing: ┤
  #define BOX_ULR	"+" // Boxdrawing: ┴
  #define BOX_UL	"+" // Boxdrawing: ┘
  #define BOX_DLR	"+" // Boxdrawing: ┘
  #define BOX_DL	"+" // Boxdrawing: ┘
#endif
