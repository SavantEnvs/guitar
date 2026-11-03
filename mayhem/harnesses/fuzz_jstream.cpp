// Fuzz jstream::Reader, the pull JSON parser Guitar uses for AI-provider replies and its model/config files.
#include "common/jstream.h"
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
	if (size < 1) return -1;
	uint8_t flags = data[0];
	char const *p = reinterpret_cast<char const *>(data + 1);
	jstream::Reader r(p, p + (size - 1));
	r.allow_comment(flags & 1);
	r.allow_ambiguous_comma(flags & 2);
	r.allow_unquoted_key(flags & 4);
	r.allow_hexadecimal(flags & 8);
	r.allow_special_constant(flags & 16);
	r.allow_key_in_array(flags & 32);
	size_t steps = 0;
	while (r.next() && steps++ < 100000) {
		(void)r.path();
		(void)r.key();
		if (r.isstring()) (void)r.string();
		if (r.isnumber()) (void)r.number();
		(void)r.match("{items{item{**");
	}
	return 0;
}
