#pragma once

#include <QString>
#include <QStringList>

namespace gitcore {

// Mẫu .gitignore dựng sẵn cho vài loại dự án thông dụng.
class GitIgnoreTemplates {
public:
    [[nodiscard]] static QStringList names();
    // Nội dung của mẫu; chuỗi rỗng nếu không có mẫu tên đó.
    [[nodiscard]] static QString content(const QString &name);
};

} // namespace gitcore
