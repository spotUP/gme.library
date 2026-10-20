// Test-tool only: std::vector in render.cpp and Effects_Buffer reference __throw_length_error and
// __throw_bad_alloc. Either one pulls libstdc++ functexcept.o, which defines both (clash) and drags
// in system_error (needs __xpg_strerror_r). With -fno-exceptions they can only abort.
#include <stdlib.h>
namespace std {
void __throw_length_error(const char*) { abort(); }
void __throw_bad_alloc() { abort(); }
}
