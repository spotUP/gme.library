/* types and includes shared by driver.h and the backends */
#ifndef DRIVER_TYPES_H
#define DRIVER_TYPES_H
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define FMT_S8 0
#define FMT_U8 1
#define FMT_S16BE 2
#define D_MAXSONGS 256

struct DInfo {
	int channels, songs, defaultSong, ntsc;
	int duration[D_MAXSONGS];
	int loop[D_MAXSONGS];
	char title[128], author[128], date[32], system[64], game[128], copyright[128];
};


static char drv_line[512];
static void say(const char *s) { size_t n = strlen(s); if (write(1, s, n) < 0) {} }
static void err(const char *s) { size_t n = strlen(s); if (write(2, s, n) < 0) {} }
#define SAYF(...) do { snprintf(drv_line, sizeof(drv_line), __VA_ARGS__); say(drv_line); } while (0)

#endif
