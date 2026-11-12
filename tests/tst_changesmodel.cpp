#include <ChangesModel.h>

#include <QtTest>

using namespace gitapp;
using namespace gitcore;

static FileChange make(const QString &path, QChar idx, QChar wt,
                       ChangeKind kind = ChangeKind::Ordinary)
{
    FileChange fc;
    fc.path = path;
    fc.indexStatus = idx;
    fc.worktreeStatus = wt;
    fc.kind = kind;
    return fc;
}

class TstChangesModel : public QObject {
    Q_OBJECT
private slots:
    void checkState_defaultsToIncludedAndIsSetPerPath();
    void statusLabel_picksMostRelevantChar();
    void roles_exposeExpectedData();
    void samePaths_emitsDataChangedWithoutReset();
    void differentPaths_resetsModel();
    void rowOfPath_findsOrReturnsMinusOne();
};

void TstChangesModel::checkState_defaultsToIncludedAndIsSetPerPath()
{
    ChangesModel m;
    m.setChanges({make("a", '.', 'M'), make("b", 'M', '.')});
    QCOMPARE(m.checkStateOfPath("a"), 2);  // mặc định chọn hết, bất kể trạng thái stage của git
    QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
    m.setCheckState("a", 0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(m.data(m.index(0), ChangesModel::CheckStateRole).toInt(), 0);
    QCOMPARE(m.data(m.index(1), ChangesModel::CheckStateRole).toInt(), 2);
    m.setCheckState("a", 0);  // không đổi -> không phát thêm
    QCOMPARE(changed.count(), 1);
    m.setCheckState("b", 1);
    QCOMPARE(m.data(m.index(1), ChangesModel::CheckStateRole).toInt(), 1);
    m.setCheckState("a", 2);
    QCOMPARE(m.checkStateOfPath("a"), 2);

    // Trạng thái được giữ khi danh sách làm mới mà file vẫn còn.
    m.setChanges({make("a", '.', 'M'), make("b", 'M', '.'), make("c", '?', '?', ChangeKind::Untracked)});
    QCOMPARE(m.checkStateOfPath("b"), 1);
    QCOMPARE(m.data(m.index(2), ChangesModel::CheckStateRole).toInt(), 2);  // file mới: chọn sẵn
}

void TstChangesModel::statusLabel_picksMostRelevantChar()
{
    QCOMPARE(ChangesModel::statusLabel(make("a", '.', 'M')), "M");
    QCOMPARE(ChangesModel::statusLabel(make("a", 'A', '.')), "A");
    QCOMPARE(ChangesModel::statusLabel(make("a", 'D', '.')), "D");
    QCOMPARE(ChangesModel::statusLabel(make("a", '?', '?', ChangeKind::Untracked)), "?");
    QCOMPARE(ChangesModel::statusLabel(make("a", 'U', 'U', ChangeKind::Unmerged)), "U");
    QCOMPARE(ChangesModel::statusLabel(make("a", 'R', '.', ChangeKind::Renamed)), "R");
}

void TstChangesModel::roles_exposeExpectedData()
{
    ChangesModel m;
    FileChange fc = make("src/x.cpp", 'M', '.');
    m.setChanges({fc});
    QCOMPARE(m.rowCount(), 1);
    QCOMPARE(m.data(m.index(0), ChangesModel::PathRole).toString(), "src/x.cpp");
    QCOMPARE(m.data(m.index(0), ChangesModel::CheckStateRole).toInt(), 2);
    QCOMPARE(m.data(m.index(0), ChangesModel::StatusRole).toString(), "M");
    QVERIFY(!m.data(m.index(5), ChangesModel::PathRole).isValid());
    QCOMPARE(m.roleNames().value(ChangesModel::CheckStateRole), QByteArray("checkState"));
}

void TstChangesModel::samePaths_emitsDataChangedWithoutReset()
{
    ChangesModel m;
    m.setChanges({make("a", '.', 'M'), make("b", '.', 'M')});
    QSignalSpy reset(&m, &QAbstractItemModel::modelReset);
    QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
    m.setChanges({make("a", 'M', '.'), make("b", '.', 'M')});
    QCOMPARE(reset.count(), 0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(m.data(m.index(0), ChangesModel::CheckStateRole).toInt(), 2);
}

void TstChangesModel::differentPaths_resetsModel()
{
    ChangesModel m;
    m.setChanges({make("a", '.', 'M')});
    QSignalSpy reset(&m, &QAbstractItemModel::modelReset);
    m.setChanges({make("a", '.', 'M'), make("b", '.', 'M')});
    QCOMPARE(reset.count(), 1);
    QCOMPARE(m.rowCount(), 2);
}

void TstChangesModel::rowOfPath_findsOrReturnsMinusOne()
{
    ChangesModel m;
    m.setChanges({make("a", '.', 'M'), make("b", '.', 'M')});
    QCOMPARE(m.rowOfPath("b"), 1);
    QCOMPARE(m.rowOfPath("zzz"), -1);
}

QTEST_APPLESS_MAIN(TstChangesModel)
#include "tst_changesmodel.moc"
