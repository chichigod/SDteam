#pragma once
#include <stdbool.h>
#include "readframe.h"

/*   used in readframe.c */
bool frame_reader_init(const char *path);
bool readframe_from_sd(table_frame_t *p);
