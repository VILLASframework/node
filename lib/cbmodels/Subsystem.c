VERSION:
3.001

#include "Subsystem.h"

STATIC:

#define SECTION STATIC
  #include "model.c"
#undef SECTION

RAM_FUNCTIONS:

#define SECTION RAM_FUNCTIONS
  #include "model.c"
#undef SECTION

RAM:

#define SECTION RAM
  #include "model.c"
#undef SECTION

model_ram();

CODE:

#define SECTION CODE
  #include "model.c"
#undef SECTION

model_code();