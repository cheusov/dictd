#include "dictP.h"

#include <stdarg.h>
#include <stddef.h>

/*
  partial snprintf implementation:
  - size PARAMETER IS COMPLETELY IGNORED
*/

int snprintf(char *str, size_t size, const char *format, ...)
{
   va_list ap;

   va_start (ap, format);
   vsprintf (str, format, ap);
   va_end (ap);
}
