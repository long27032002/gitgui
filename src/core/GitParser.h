#pragma once

#include "GitTypes.h"

namespace gitcore {

class GitParser {
public:
    // Parse đầu ra của `git status --porcelain=v2 --branch -z`.
    [[nodiscard]] static RepoStatus parseStatusV2(const QByteArray &raw);

    // `git for-each-ref --format=%(refname)\t%(refname:short)\t%(upstream:short)\t%(HEAD)`
    [[nodiscard]] static QVector<BranchInfo> parseBranches(const QByteArray &raw);
    // `git log --format=%H%x1f%h%x1f%an%x1f%at%x1f%P%x1f%s%x1f%D%x1e`
    [[nodiscard]] static QVector<CommitInfo> parseLog(const QByteArray &raw);
    // `git show --format= --name-status -z`
    [[nodiscard]] static QVector<CommitFile> parseNameStatus(const QByteArray &raw);
};

} // namespace gitcore
