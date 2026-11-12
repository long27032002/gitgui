#pragma once

#include "ConflictModels.h"

#include <GitConflict.h>
#include <GitService.h>

#include <QDateTime>
#include <QObject>

#include <optional>

namespace gitapp {

// Giải quyết conflict: chọn file, chọn cách xử lý từng khối, rồi yêu cầu ghi kết quả.
// Lớp này chỉ đọc và tính toán; việc ghi/`git add` do RepoController thực hiện qua các signal.
//
// "Của tôi" luôn là công việc của người dùng: khi merge/cherry-pick/revert đó là phía `ours`
// của git, còn khi rebase thì ngược lại (`theirs` — commit của mình đang được áp lại).
class ConflictController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *files READ files CONSTANT)
    Q_PROPERTY(QAbstractItemModel *blocks READ blocks CONSTANT)
    Q_PROPERTY(QString selectedPath READ selectedPath NOTIFY fileChanged)
    Q_PROPERTY(QString description READ description NOTIFY fileChanged)
    Q_PROPERTY(QString mineAction READ mineAction NOTIFY fileChanged)
    Q_PROPERTY(QString incomingAction READ incomingAction NOTIFY fileChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY fileChanged)
    Q_PROPERTY(bool binary READ binary NOTIFY fileChanged)
    // Một bên đã xoá file (xung đột xoá/sửa): chỉ có cách chọn giữ hoặc xoá file.
    Q_PROPERTY(bool deleteConflict READ deleteConflict NOTIFY fileChanged)
    Q_PROPERTY(bool hasBlocks READ hasBlocks NOTIFY fileChanged)
    Q_PROPERTY(bool hasMarkers READ hasMarkers NOTIFY fileChanged)
    Q_PROPERTY(int blockCount READ blockCount NOTIFY fileChanged)
    Q_PROPERTY(int chosenCount READ chosenCount NOTIFY choicesChanged)
    Q_PROPERTY(bool allChosen READ allChosen NOTIFY choicesChanged)

public:
    explicit ConflictController(QObject *parent = nullptr);

    [[nodiscard]] QAbstractItemModel *files() { return &m_files; }
    [[nodiscard]] QAbstractItemModel *blocks() { return &m_blocks; }
    [[nodiscard]] const QString &selectedPath() const { return m_selectedPath; }
    [[nodiscard]] const QString &description() const { return m_description; }
    [[nodiscard]] const QString &mineAction() const { return m_mineAction; }
    [[nodiscard]] const QString &incomingAction() const { return m_incomingAction; }
    [[nodiscard]] bool loading() const { return m_loading; }
    [[nodiscard]] bool binary() const { return m_binary; }
    [[nodiscard]] bool deleteConflict() const { return !m_selectedPath.isEmpty() && !(m_mineHasFile && m_incomingHasFile); }
    [[nodiscard]] bool hasBlocks() const { return m_blocks.rowCount() > 0; }
    [[nodiscard]] bool hasMarkers() const { return m_hasMarkers; }
    [[nodiscard]] int blockCount() const { return m_blocks.rowCount(); }
    [[nodiscard]] int chosenCount() const { return m_blocks.chosenCount(); }
    [[nodiscard]] bool allChosen() const { return hasBlocks() && chosenCount() == blockCount(); }

    // Gọi sau mỗi lần làm mới trạng thái repo. `unmerged` là các mục Unmerged của status.
    void setState(std::optional<gitcore::GitService> service, gitcore::OperationKind op,
                  QVector<gitcore::FileChange> unmerged);

    Q_INVOKABLE void select(int row);
    // choice: 0 bỏ chọn, 1 bản của tôi, 2 bản của họ, 3 giữ cả hai (của tôi trước)
    Q_INVOKABLE void choose(int block, int choice);
    Q_INVOKABLE void applyChoices();
    Q_INVOKABLE void resolveWholeFile(bool mine);
    Q_INVOKABLE void markResolved();
    Q_INVOKABLE void reload();
    // Tải lại nếu file đã bị sửa bên ngoài (gọi khi cửa sổ được kích hoạt lại).
    Q_INVOKABLE void reloadIfChanged();
    Q_INVOKABLE void openInEditor();

signals:
    void fileChanged();
    void choicesChanged();
    void resolveContentRequested(const QString &path, const QByteArray &content);
    void resolveSideRequested(const QString &path, bool useOurs);
    void markResolvedRequested(const QString &path);
    void errorOccurred(const QString &message);

private:
    void loadFile(const QString &path);
    void clearFile();
    [[nodiscard]] bool mineIsOurs() const { return m_op != gitcore::OperationKind::Rebase; }

    std::optional<gitcore::GitService> m_service;
    gitcore::OperationKind m_op = gitcore::OperationKind::None;
    QVector<gitcore::FileChange> m_unmerged;

    ConflictFilesModel m_files;
    ConflictBlocksModel m_blocks;

    QString m_selectedPath;
    QString m_description;
    QString m_mineAction;
    QString m_incomingAction;
    bool m_loading = false;
    bool m_binary = false;
    bool m_hasMarkers = false;
    bool m_mineHasFile = true;
    bool m_incomingHasFile = true;

    gitcore::ConflictFile m_file;
    QDateTime m_loadedMtime;
    qint64 m_loadedSize = -1;
    quint64 m_loadGen = 0;
};

} // namespace gitapp
