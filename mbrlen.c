#include <wchar.h>
#include <assert.h>

/*
  partial mbrlen implementation:
  - doesn't change *ps
  - ignores n
  - s should not be NULL
  - tests first char only
 */
size_t mbrlen (const char *s, size_t n, mbstate_t *ps)
{
   int c;

   assert (s); /* not implemented */

   c = *(const unsigned char *)s;

   if (c == 0)
      return 0;
   else if (c <= 0x7F)
      return 1;
   else if (c <= 0xBF)
      return (size_t) -1;
   else if (c <= 0xDF)
      return 2;
   else if (c <= 0xEF)
      return 3;
   else if (c <= 0xF7)
      return 4;
   else if (c <= 0xFB)
      return 5;
   else if (c <= 0xFD)
      return 6;
   else
      return (size_t) -1;
}
