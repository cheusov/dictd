#include <wchar.h>
#include <stdlib.h>

/*
  partial mbrtowc implementation:
  - doesn't change *ps
  - s should NOT be NULL
*/

int wctomb (char *s, wchar_t wc)
{
   return (int) wcrtomb (s, wc, NULL);
}
