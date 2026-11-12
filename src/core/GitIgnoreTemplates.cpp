#include "GitIgnoreTemplates.h"

#include <QHash>

namespace gitcore {

namespace {

const QList<QPair<QString, QString>> &table()
{
    static const QList<QPair<QString, QString>> t = {
        {QStringLiteral("Qt / C++"), QStringLiteral(
            "# Build\n/build/\n/build-*/\n/cmake-build-*/\nCMakeCache.txt\nCMakeFiles/\ncmake_install.cmake\n"
            "compile_commands.json\n*.o\n*.obj\n*.a\n*.so\n*.so.*\n*.dll\n*.exe\n*.out\n\n"
            "# Qt\n*.pro.user\n*.pro.user.*\n*.qbs.user\n*.autosave\nmoc_*.cpp\nmoc_*.h\nqrc_*.cpp\nui_*.h\n.qt/\n\n"
            "# Editor\n.vscode/\n.idea/\n*.swp\n*~\n")},
        {QStringLiteral("CMake"), QStringLiteral(
            "/build/\n/build-*/\n/out/\nCMakeCache.txt\nCMakeFiles/\ncmake_install.cmake\nCTestTestfile.cmake\n"
            "install_manifest.txt\ncompile_commands.json\n.cache/\n")},
        {QStringLiteral("Python"), QStringLiteral(
            "__pycache__/\n*.py[cod]\n*.egg-info/\n.eggs/\nbuild/\ndist/\n.venv/\nvenv/\nenv/\n.pytest_cache/\n"
            ".mypy_cache/\n.ruff_cache/\n.coverage\nhtmlcov/\n.ipynb_checkpoints/\n")},
        {QStringLiteral("Node"), QStringLiteral(
            "node_modules/\nnpm-debug.log*\nyarn-error.log*\n.pnpm-store/\ndist/\nbuild/\ncoverage/\n.env\n.env.*\n"
            ".next/\n.nuxt/\n*.tsbuildinfo\n")},
        {QStringLiteral("Rust"), QStringLiteral("/target/\n**/*.rs.bk\nCargo.lock\n")},
        {QStringLiteral("Go"), QStringLiteral("/bin/\n/vendor/\n*.exe\n*.test\n*.out\ngo.work\n")},
        {QStringLiteral("Java"), QStringLiteral(
            "*.class\n*.jar\n*.war\n/target/\n/build/\n.gradle/\n.idea/\n*.iml\nout/\n")},
        {QStringLiteral("Linux / Trình soạn thảo"), QStringLiteral(
            "*~\n*.swp\n*.swo\n.directory\n.Trash-*\n.DS_Store\nThumbs.db\n.idea/\n.vscode/\n*.log\n")},
    };
    return t;
}

} // namespace

QStringList GitIgnoreTemplates::names()
{
    QStringList out;
    for (const auto &kv : table())
        out << kv.first;
    return out;
}

QString GitIgnoreTemplates::content(const QString &name)
{
    for (const auto &kv : table())
        if (kv.first == name)
            return kv.second;
    return {};
}

} // namespace gitcore
