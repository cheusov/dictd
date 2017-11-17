/*
  partial mbrtowc implementation:
  - doesn't change *ps
  - s should NOT be NULL
*/

#include "dictP.h"

#include <wchar.h>
#include <wctype.h>

#include <stdlib.h>

int wctomb__ (char *s, wchar_t wc)
{
   return (int) wcrtomb__ (s, wc, NULL);
}
