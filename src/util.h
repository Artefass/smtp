#pragma once
#include <stdlib.h>
#define ARRAYNUM(x) (sizeof(x) / sizeof((x)[0]))
void get_random(size_t size, char *buf);