// C runtime glue for GME inside a library: no startup code, no libnix heap
// (its malloc needs the startup's pool and exit chain). Every allocation is an
// exec AllocVec, so each GME handle's memory belongs to nobody but the handle.
#include <stddef.h>
#include <string.h>
#include <new>
#include <exec/memory.h>
#include <exec/alerts.h>
#include <proto/exec.h>

extern "C" {

// Count of failed allocations; GME_Open compares it before and after loading
// to tell "out of memory" from "not our module". Diagnostic only.
ULONG gmelib_allocfails;

void *malloc(size_t n)
{
	void *p = AllocVec((ULONG) (n != 0 ? n : 1), MEMF_PUBLIC);
	if (p == NULL)
		gmelib_allocfails++;
	return p;
}

void *calloc(size_t count, size_t n)
{
	void *p = AllocVec((ULONG) (count * n != 0 ? count * n : 1), MEMF_PUBLIC | MEMF_CLEAR);
	if (p == NULL)
		gmelib_allocfails++;
	return p;
}

void free(void *p)
{
	if (p != NULL)
		FreeVec(p);
}

// AllocVec keeps the allocation size (header included) in the longword in
// front of the block.
void *realloc(void *old, size_t n)
{
	void *p;
	size_t have;
	if (old == NULL)
		return malloc(n);
	if (n == 0) {
		free(old);
		return NULL;
	}
	have = ((ULONG *) old)[-1] - sizeof(ULONG);
	p = malloc(n);
	if (p != NULL) {
		memcpy(p, old, have < n ? have : n);
		FreeVec(old);
	}
	return p;
}

// Reached only on impossible states (asserts, pure virtual calls, vector
// overflow); the library has no exit chain to return through.
void abort(void)
{
	Alert(AT_DeadEnd | 0x474D4520UL);	// 'GME '
	for (;;)
		;
}

void __assert_func(const char *, int, const char *, const char *)
{
	abort();
}

void __cxa_pure_virtual(void)
{
	abort();
}

}

namespace std {
const nothrow_t nothrow = nothrow_t();
void __throw_length_error(const char *) { abort(); }
void __throw_bad_alloc() { abort(); }
}

void *operator new(size_t n) { return malloc(n); }
void *operator new[](size_t n) { return malloc(n); }
void *operator new(size_t n, const std::nothrow_t &) noexcept { return malloc(n); }
void *operator new[](size_t n, const std::nothrow_t &) noexcept { return malloc(n); }
void operator delete(void *p) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }
