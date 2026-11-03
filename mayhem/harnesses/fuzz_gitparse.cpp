// Fuzz the text parsers Guitar applies to `git` command output and raw object contents:
// commit objects, the custom log line format, tree listings, unified diffs and branch ahead/behind info.
#include "quiet.h"
#include "Git.h"
#include "GitDiffManager.h"
#include "GitObjectManager.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	install_quiet_qt_handler();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
	if (size < 1) return -1;
	int which = data[0] % 5;
	char const *p = reinterpret_cast<char const *>(data + 1);
	size_t n = size - 1;
	switch (which) {
	case 0:
		Git::parseCommit(std::vector<char>(p, p + n));
		break;
	case 1:
		Git::parseCommitItem(std::string(p, n));
		break;
	case 2: {
		QByteArray ba(p, (qsizetype)n);
		parseGitTreeObject(ba, "");
		parseGitTreeObject(ba, "dir");
		break;
	}
	case 3: {
		GitDiff info;
		GitDiffManager::parseDiff(std::string(p, n), info);
		break;
	}
	case 4: {
		GitBranch b;
		Git::parseAheadBehind(std::string(p, n), &b);
		break;
	}
	}
	return 0;
}
