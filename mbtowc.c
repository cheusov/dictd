#include <wchar.h>
#include <stdlib.h>

/*
  partial mbrtowc implementation:
  - ignores n
  - s should NOT be NULL
  - pwc should NOT be NULL
*/
int mbtowc (wchar_t *pwc, const char *s, size_t n)
{
   return (int) mbrtowc (pwc, s, n, NULL);
}
