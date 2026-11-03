// Fuzz gzip::decompress (RFC 1952 container + inflate) over an in-memory reader/writer.
#include "gzip.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
class MemReader : public AbstractSimpleReader {
	uint8_t const *data_;
	int64_t size_;
	int64_t pos_ = 0;
public:
	MemReader(uint8_t const *data, size_t size) : data_(data), size_((int64_t)size) {}
	int read(void *ptr, int64_t len) override
	{
		int64_t n = std::min<int64_t>(len, size_ - pos_);
		if (n <= 0) return 0;
		memcpy(ptr, data_ + pos_, (size_t)n);
		pos_ += n;
		return (int)n;
	}
	int64_t pos() const override { return pos_; }
	void seek(int64_t pos) override { pos_ = pos; }
};

class CountWriter : public AbstractSimpleWriter {
public:
	int64_t total = 0;
	int write(void const *, int64_t len) override
	{
		total += len;
		return (int)len;
	}
};
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
	if (size < 1) return -1;
	// always bounded: an unbounded inflate bomb would only report libFuzzer's 2 GiB rss limit
	int64_t max_size = (data[0] & 1) ? (4 << 20) : (int64_t)(data[0] >> 1) * 1024;
	MemReader in(data + 1, size - 1);
	CountWriter out;
	gzip z;
	z.set_maximul_size(max_size);
	z.decompress(&in, &out);
	return 0;
}
