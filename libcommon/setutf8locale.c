/*
 * Created by Aleksey Cheusov <vle@gmx.net>
 * Public Domain
 */

#include <locale.h>
#include <maa.h>

#include "common.h"

static const char *const utf8_locales [] = {
   "UTF-8",
   "C.UTF-8",
   "en_US.UTF-8",
};

void setutf8locale(void)
{
	for (size_t i = 0 ; i < sizeof(utf8_locales)/sizeof(utf8_locales[0]); ++i) {
		if (setlocale(LC_CTYPE, utf8_locales[i]))
			return;
	}

	err_fatal (__func__, "Cannot set UTF-8 locale. Try to use --locale option\n");
}
