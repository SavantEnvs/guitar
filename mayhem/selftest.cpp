// Known-answer self-test over the same Guitar sources the fuzz harnesses exercise.
// Prints "PASS <name>" per satisfied check; test.sh counts them against the expected total.
#include "Git.h"
#include "GitDiffManager.h"
#include "GitObjectManager.h"
#include "GitPack.h"
#include "GitPackIdxV2.h"
#include "common/jstream.h"
#include "gzip.h"
#include "zip/zipinternal.h"
#include <QBuffer>
#include <QByteArray>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

static int g_pass = 0, g_fail = 0;
static void check(bool ok, char const *name)
{
	if (ok) {
		g_pass++;
		printf("PASS %s\n", name);
	} else {
		g_fail++;
		printf("FAIL %s\n", name);
	}
}

namespace {
class MemReader : public AbstractSimpleReader {
	std::vector<uint8_t> const &d_;
	int64_t pos_ = 0;
public:
	explicit MemReader(std::vector<uint8_t> const &d) : d_(d) {}
	int read(void *ptr, int64_t len) override
	{
		int64_t n = std::min<int64_t>(len, (int64_t)d_.size() - pos_);
		if (n <= 0) return 0;
		memcpy(ptr, d_.data() + pos_, (size_t)n);
		pos_ += n;
		return (int)n;
	}
	int64_t pos() const override { return pos_; }
	void seek(int64_t p) override { pos_ = p; }
};
class StrWriter : public AbstractSimpleWriter {
public:
	std::string out;
	int write(void const *ptr, int64_t len) override
	{
		out.append((char const *)ptr, (size_t)len);
		return (int)len;
	}
};
}

static std::vector<uint8_t> const GZ_FOX = {31,139,8,0,0,0,0,0,2,255,11,201,72,85,40,44,205,76,206,86,72,42,202,47,207,83,72,203,175,80,200,42,205,45,40,86,200,47,75,45,82,40,1,74,231,36,86,85,42,164,228,167,115,1,0,56,193,147,109,44,0,0,0};
static std::vector<uint8_t> const ZIP_TWO = {80,75,3,4,20,0,0,0,8,0,131,24,34,80,72,201,17,72,12,0,0,0,10,0,0,0,9,0,0,0,104,101,108,108,111,46,116,120,116,203,72,205,201,201,87,168,202,44,224,2,0,80,75,3,4,20,0,0,0,8,0,131,24,34,80,52,129,92,47,23,0,0,0,144,0,0,0,11,0,0,0,100,105,114,47,115,117,98,46,116,120,116,43,46,77,82,72,203,204,73,85,72,206,207,43,73,205,43,41,230,42,30,80,17,0,80,75,1,2,20,3,20,0,0,0,8,0,131,24,34,80,72,201,17,72,12,0,0,0,10,0,0,0,9,0,0,0,0,0,0,0,0,0,0,0,128,1,0,0,0,0,104,101,108,108,111,46,116,120,116,80,75,1,2,20,3,20,0,0,0,8,0,131,24,34,80,52,129,92,47,23,0,0,0,144,0,0,0,11,0,0,0,0,0,0,0,0,0,0,0,128,1,51,0,0,0,100,105,114,47,115,117,98,46,116,120,116,80,75,5,6,0,0,0,0,2,0,2,0,112,0,0,0,115,0,0,0,0,0};

static void test_gzip()
{
	std::string const fox = "The quick brown fox jumps over the lazy dog\n";
	{
		MemReader in(GZ_FOX);
		StrWriter out;
		gzip z;
		check(z.decompress(&in, &out) && out.out == fox, "gzip decompress known text");
	}
	{
		std::vector<uint8_t> bad = GZ_FOX;
		bad[0] = 0x00;
		MemReader in(bad);
		StrWriter out;
		gzip z;
		check(!z.decompress(&in, &out) && out.out.empty(), "gzip rejects bad magic");
	}
	{
		MemReader in(GZ_FOX);
		StrWriter out;
		gzip z;
		z.set_maximul_size(10);
		bool ok = z.decompress(&in, &out);
		check(!ok || out.out.size() < fox.size(), "gzip honours maximum size");
	}
}

static void test_zip()
{
	char const *b = (char const *)ZIP_TWO.data();
	zip::ZipInternal z;
	check(z.attach(b, b + ZIP_TWO.size()) && z.size() == 2, "zip attach lists two entries");
	std::vector<char> out;
	bool ok = z.extract_file("hello.txt", &out);
	check(ok && std::string(out.begin(), out.end()) == "hello zip\n", "zip extract hello.txt");
	out.clear();
	ok = z.extract_file("dir/sub.txt", &out);
	std::string want;
	for (int i = 0; i < 8; i++) want += "sub file contents\n";
	check(ok && std::string(out.begin(), out.end()) == want, "zip extract dir/sub.txt (inflate)");
	zip::ZipInternal bad;
	std::string junk(64, 'x');
	check(!bad.attach(junk.data(), junk.data() + junk.size()), "zip rejects garbage");
}

static void test_jstream()
{
	std::string js = R"({"items":[{"name":"a","n":1},{"name":"b","n":2500}],"ok":true})";
	jstream::Reader r(js.data(), js.data() + js.size());
	std::vector<std::string> names;
	double sum = 0;
	bool saw_ok = false;
	while (r.next()) {
		if (r.match("{items[{name")) names.push_back(r.string());
		if (r.match("{items[{n")) sum += r.number();
		if (r.match("{ok")) saw_ok = r.isnumber() || r.isstring() || true;
	}
	check(names.size() == 2 && names[0] == "a" && names[1] == "b", "jstream extracts string values by path");
	check(sum == 2501, "jstream extracts numbers by path");
	check(saw_ok, "jstream reaches trailing key");
	std::string esc = R"({"s":"aé\n"})";
	jstream::Reader r2(esc.data(), esc.data() + esc.size());
	std::string got;
	while (r2.next()) if (r2.match("{s")) got = r2.string();
	check(got == "a\xc3\xa9\n", "jstream decodes escapes");
}

static void test_git_text()
{
	std::string c =
		"tree 4b825dc642cb6eb9a060e54bf8d69288fbee4904\n"
		"parent 1111111111111111111111111111111111111111\n"
		"author Alice Example <alice@example.com> 1600000000 +0900\n"
		"committer Bob <bob@example.com> 1600000100 +0000\n"
		"\nFirst line\nSecond line\n";
	auto item = Git::parseCommit(std::vector<char>(c.begin(), c.end()));
	check(item && item->tree.toString() == "4b825dc642cb6eb9a060e54bf8d69288fbee4904", "parseCommit tree id");
	check(item && item->parent_ids.size() == 1 && item->parent_ids[0].toString() == "1111111111111111111111111111111111111111", "parseCommit parent");
	check(item && item->author == "Alice Example" && item->email == "alice@example.com", "parseCommit author/email");
	check(item && item->message == "First line\nSecond line", "parseCommit message");
	std::string nt = "author X <x@y> 1 +0000\n\nmsg\n";
	check(!Git::parseCommit(std::vector<char>(nt.begin(), nt.end())), "parseCommit rejects missing tree");

	auto li = Git::parseCommitItem("id:0123456789abcdef0123456789abcdef01234567#gpg:G#parent:1111111111111111111111111111111111111111 2222222222222222222222222222222222222222#author:Alice#mail:a@example.com#date:2020-01-02 03:04:05 +0900##Fix a bug");
	check(li && li->commit_id.toString() == "0123456789abcdef0123456789abcdef01234567" && li->parent_ids.size() == 2, "parseCommitItem id and parents");
	check(li && li->author == "Alice" && li->email == "a@example.com" && li->message == "Fix a bug" && li->sign.verify == 'G', "parseCommitItem fields");
	check(!Git::parseCommitItem("no separator here"), "parseCommitItem rejects malformed line");

	GitBranch b;
	Git::parseAheadBehind("[ahead 3, behind 12]", &b);
	check(b.ahead == 3 && b.behind == 12, "parseAheadBehind counts");

	QByteArray tree("100644 blob 0123456789abcdef0123456789abcdef01234567\tREADME.md\n040000 tree 89abcdef0123456789abcdef0123456789abcdef\tsrc\n");
	GitTreeItemList tl = parseGitTreeObject(tree, "d");
	check(tl.size() == 2 && tl[0].type == GitTreeItem::BLOB && tl[0].name == "d/README.md" && tl[1].type == GitTreeItem::TREE && tl[1].id == "89abcdef0123456789abcdef0123456789abcdef", "parseGitTreeObject entries");

	GitDiff info;
	info.path = "f.c";
	GitDiff d = GitDiffManager::parseDiff("--- a/f.c\n+++ b/f.c\n@@ -1,3 +1,3 @@\n int a;\n-int b;\n+int c;\n@@ -10 +10,2 @@\n x\n+y\n", info);
	check(d.hunks.size() == 2 && d.hunks[0].at == "@@ -1,3 +1,3 @@" && d.hunks[0].lines.size() == 3 && d.hunks[1].lines.size() == 2, "parseDiff hunks and lines");
}

static QByteArray zdeflate(std::string const &s)
{
	uLongf n = compressBound((uLong)s.size());
	QByteArray out((qsizetype)n, 0);
	compress((Bytef *)out.data(), &n, (Bytef const *)s.data(), (uLong)s.size());
	out.resize((qsizetype)n);
	return out;
}

static void test_pack()
{
	QByteArray pack;
	pack.append("PACK", 4);
	char const hdr[8] = {0, 0, 0, 2, 0, 0, 0, 1};
	pack.append(hdr, 8);
	pack.append((char)0x35); // blob, size 5
	pack.append(zdeflate("hello"));
	QBuffer buf(&pack);
	buf.open(QIODevice::ReadOnly);
	GitPackIdxItem item;
	memset(item.id, 0, sizeof(item.id));
	item.offset = 12;
	GitPackObject obj;
	check(GitPack::load(&buf, &item, &obj) && obj.type == GitObject::Type::BLOB && obj.content == QByteArray("hello") && obj.expanded_size == 5, "GitPack::load blob");

	QByteArray tmp("tree 12\0data", 12);
	GitObject::Type t = GitPack::stripHeader(&tmp);
	check(t == GitObject::Type::TREE && tmp == QByteArray("data"), "GitPack::stripHeader tree");

	QByteArray raw;
	raw.append("100644 a.txt", 12);
	raw.append('\0');
	for (int i = 0; i < 20; i++) raw.append((char)i);
	raw.append("tail");
	GitPack::decodeTree(&raw);
	check(raw == QByteArray("100644 blob 000102030405060708090a0b0c0d0e0f10111213\ta.txt\n"), "GitPack::decodeTree entry text");

	QByteArray base("hello world");
	char const dl[] = {11, 8, (char)0x90, 5, 3, 'X', 'Y', 'Z'};
	QByteArray delta(dl, sizeof(dl));
	check(GitObjectManager::applyDelta(&base, &delta) == QByteArray("helloXYZ"), "applyDelta copy+insert");
	char const bad[] = {11, 8, (char)0x91, 50, (char)0x91, 50};
	QByteArray baddelta(bad, sizeof(bad));
	check(GitObjectManager::applyDelta(&base, &baddelta).isEmpty(), "applyDelta rejects out-of-range copy");
}

int main()
{
	qInstallMessageHandler([](QtMsgType, QMessageLogContext const &, QString const &) {});
	test_gzip();
	test_zip();
	test_jstream();
	test_git_text();
	test_pack();
	printf("SELFTEST passed=%d failed=%d\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
