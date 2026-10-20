/* getstats: prints GME_GetStats of gme.library (client; links no GME code).
   usage: getstats [out-file]   writes "opens=N renders=N bytes=N" to stdout and the file.
   Used by rig/run-hip.sh: the renders counter is the F6 sentinel. */
const char getstats_ver[] = "$VER: getstats 1.0 (19.10.2026)";
#include <exec/types.h>
#include <proto/exec.h>
#include <libraries/gme.h>
#include <proto/gme.h>
#include <stdio.h>

struct Library *GmeBase;

int main(int argc, char **argv)
{
	struct GMEStats st;
	FILE *f;
	GmeBase = OpenLibrary((STRPTR) GMENAME, GMELIB_VERSION);
	if (!GmeBase) { printf("no gme.library\n"); return 20; }
	GME_GetStats(&st);
	printf("opens=%lu renders=%lu bytes=%lu\n", (unsigned long) st.opens, (unsigned long) st.renders, (unsigned long) st.bytes);
	if (argc > 1 && (f = fopen(argv[1], "w")) != NULL) {
		fprintf(f, "opens=%lu renders=%lu bytes=%lu\n", (unsigned long) st.opens, (unsigned long) st.renders, (unsigned long) st.bytes);
		fclose(f);
	}
	CloseLibrary(GmeBase);
	return 0;
}
