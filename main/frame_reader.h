#pragma once
#include <stdbool.h>
#include "readframe.h"

/* Internal use only: SD card frame parsing */

bool frame_reader_init(const char *path);
bool readframe_from_sd(table_frame_t *p);
