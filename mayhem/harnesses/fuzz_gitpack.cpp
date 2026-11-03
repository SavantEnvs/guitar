// Fuzz the git packfile reader: object header + inflate, delta application, and the tree/object-header decoders.
#include "quiet.h"
#include "GitObjectManager.h"
#include "GitPack.h"
#include "GitPackIdxV2.h"
#include <QBuffer>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	install_quiet_qt_handler();
	return 0;
}

// layout: [mode:1][offset:2 LE][rest...]
//   mode 0: pack file in `rest`, object at `offset` (GitPack::load -> decode)
//   mode 1: `rest` split in half: base object | delta (GitObjectManager::applyDelta)
extern "C" int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
	if (size < 4) return -1;
	int mode = data[0] & 1;
	uint32_t offset = data[1] | (data[2] << 8);
	char const *rest = reinterpret_cast<char const *>(data + 3);
	size_t n = size - 3;
	if (mode == 0) {
		QByteArray pack(rest, (qsizetype)n);
		QBuffer buf(&pack);
		buf.open(QIODevice::ReadOnly);
		GitPackIdxItem item;
		memset(item.id, 0, sizeof(item.id));
		item.offset = offset;
		GitPackObject obj;
		if (GitPack::load(&buf, &item, &obj)) {
			QByteArray c = obj.content;
			GitObject::Type t = GitPack::stripHeader(&c);
			if (t == GitObject::Type::TREE) GitPack::decodeTree(&c);
		}
		return 0;
	}
	size_t half = n / 2;
	QByteArray base(rest, (qsizetype)half);
	QByteArray delta(rest + half, (qsizetype)(n - half));
	GitObjectManager::applyDelta(&base, &delta);
	return 0;
}
