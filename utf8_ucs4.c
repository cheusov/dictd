#include <wctype.h>
#include <stdlib.h>
#include <ctype.h>

#include "utf8_ucs4.h"

int ucs4_to_utf8 (wint_t ucs4, char *result)
{
   int i;

   if (ucs4 <= 0x7F){
      result [0] = (char) ucs4;
      result [1] = 0;
   }else{
      int octet_count = 0;
      if (ucs4 <= 0x7FF){
	 result [0]  = 0xC0L;
	 octet_count = 2;
      }else if (ucs4 <= 0xFFFFL){
	 result [0]  = 0xE0;
	 octet_count = 3;
      }else if (ucs4 <= 0x1FFFFFL){
	 result [0]  = 0xF0;
	 octet_count = 4;
      }else if (ucs4 <= 0x3FFFFFFL){
	 result [0]  = 0xF8;
	 octet_count = 5;
      }else if (ucs4 <= 0x7FFFFFFFL){
	 result [0]  = 0xFC;
	 octet_count = 6;
      }else{
/* invalid UCS4 character */
	 return 0;
      }

      result [octet_count] = 0;

      for (i = octet_count-1; i--; ){
	 result [i + 1] = 0x80 | (ucs4 & 0x3F);
	 ucs4 >>= 6;
      }

      result [0] |= ucs4;
   }

   return 1;
}

const char * utf8_to_ucs4 (const char *ptr, wint_t *result)
{
   wint_t ret;
   int ch;
   int octet_count;
   int bits_count;
   int i;

   ret = 0;

   ch = (unsigned char) *ptr++;

   if ((ch & 0x80) == 0x00){
      *result = ch;
   }else{
      if ((ch & 0xE0) == 0xC0){
	 octet_count = 2;
	 ch &= ~0xE0;
      }else if ((ch & 0xF0) == 0xE0){
	 octet_count = 3;
	 ch &= ~0xF0;
      }else if ((ch & 0xF8) == 0xF0){
	 octet_count = 4;
	 ch &= ~0xF8;
      }else if ((ch & 0xFC) == 0xF8){
	 octet_count = 5;
	 ch &= ~0xFC;
      }else if ((ch & 0xFE) == 0xFC){
	 octet_count = 6;
	 ch &= ~0xFE;
      }else{
	 return NULL;
      }

      bits_count = (octet_count-1) * 6;
      ret |= (ch << bits_count);
      for (i=1; i < octet_count; ++i){
	 bits_count -= 6;

	 ch = (unsigned char) *ptr++;
	 if ((ch & 0xC0) != 0x80){
	    return NULL;
	 }

	 ret |= ((ch & 0x3F) << bits_count);
      }

      *result = ret;
   }

   return ptr;
}

char *strlwr_8bit (char *str)
{
   char *p;
   for (p = str; *p; ++p){
      *p = tolower ((unsigned char) *p);
   }

   return str;
}

char *strupr_8bit (char *str)
{
   char *p;
   for (p = str; *p; ++p){
      *p = toupper ((unsigned char) *p);
   }

   return str;
}

typedef int (* xxx_type) (int);

static char *strxxx_utf8 (char *str, wint_t (* xxx) (wint_t))
{
   char   *p, *p_next;
   wint_t ucs4_char;
   char   char_copy;

   for (p = str; *p; ){
      p_next = (char *) utf8_to_ucs4 (p, &ucs4_char);
      if (!p_next)
	 return NULL;

      char_copy = *p_next;
      ucs4_to_utf8 (xxx (ucs4_char), p);
      *p_next = char_copy;

      p = p_next;
   }

   return str;
}

char *strlwr_utf8 (char *str)
{
   return strxxx_utf8 (str, towlower);
}

char *strupr_utf8 (char *str)
{
   return strxxx_utf8 (str, towupper);
}

size_t strlen_utf8 (const char *str)
{
   int ret = 0;
   int len = 0;
   int c;

   while (*str){
      c = *(const unsigned char *)str;

      if (c <= 0x7F)
	 len = 1;
      else if (c <= 0xBF)
	 return (size_t) -1;
      else if (c <= 0xDF)
	 len = 2;
      else if (c <= 0xEF)
	 len = 3;
      else if (c <= 0xF7)
	 len = 4;
      else if (c <= 0xFB)
	 len = 5;
      else if (c <= 0xFD)
	 len = 6;
      else
	 return (size_t) -1;

      ret += 1;
      str += len;
   }

   return ret;
}

size_t charlen_utf8 (const char *str)
{
   int c;

   c = *(const unsigned char *)str;

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
