// Fuzz the in-memory zip reader (zip::ZipInternal) used to unpack downloaded archives.
#include "quiet.h"
#include "zip/zipinternal.h"
#include <cstddef>
#include <cstdint>
#include <vector>

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	install_quiet_qt_handler();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
	char const *begin = reinterpret_cast<char const *>(data);
	zip::ZipInternal z;
	if (!z.attach(begin, begin + size)) return -1;
	for (zip::Item const &item : *z.get_item_list()) {
		std::vector<char> out;
		z.extract_file(&item.cd, &out);
	}
	return 0;
}
