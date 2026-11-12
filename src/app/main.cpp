#include "CloneController.h"
#include "LanguageController.h"
#include "RepoController.h"
#include "Theme.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>

#include <atomic>

namespace {

std::atomic<bool> g_qmlWarning{false};
QtMessageHandler g_previousHandler = nullptr;

// Ở chế độ smoke-test, mọi cảnh báo QML (thiếu module, binding lỗi...) làm test thất bại.
void smokeMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if ((type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)
        && (msg.contains(QStringLiteral(".qml")) || msg.contains(QStringLiteral("module"))))
        g_qmlWarning = true;
    if (g_previousHandler)
        g_previousHandler(type, ctx, msg);
}

// Giá trị đi kèm một tuỳ chọn dòng lệnh, ví dụ `--tab 1`.
QString optionValue(const QStringList &args, const QString &name)
{
    const qsizetype i = args.indexOf(name);
    return (i >= 0 && i + 1 < args.size()) ? args.at(i + 1) : QString();
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("gitgui"));
    QGuiApplication::setOrganizationName(QStringLiteral("gitgui"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // `--smoke-test [repo]`: dựng giao diện, mở mọi hộp thoại rồi thoát; cảnh báo QML = thất bại.
    // `--screenshot file.png [repo] [--tab N] [--light] [--dialog tên] [--toggle-diff-row N]`: chụp cửa sổ ra ảnh (kiểm tra giao diện).
    const QStringList args = app.arguments();
    const qsizetype smokeIdx = args.indexOf(QStringLiteral("--smoke-test"));
    const qsizetype shotIdx = args.indexOf(QStringLiteral("--screenshot"));
    const bool smoke = smokeIdx >= 0;
    const bool shot = shotIdx >= 0;
    const bool headless = smoke || shot;
    if (smoke)
        g_previousHandler = qInstallMessageHandler(smokeMessageHandler);

    QSettings settings;
    gitapp::Theme theme;
    const bool wantDark = shot ? !args.contains(QStringLiteral("--light"))
                               : settings.value(QStringLiteral("theme/dark"), true).toBool();
    theme.setDark(wantDark);
    theme.apply();
    if (!headless)
        QObject::connect(&theme, &gitapp::Theme::changed, &app,
                         [&] { settings.setValue(QStringLiteral("theme/dark"), theme.dark()); });

    gitapp::RepoController repo;
    repo.setTheme(&theme);
    if (!headless)
        repo.recent()->enablePersistence();

    gitapp::CloneController cloner;
    // Clone xong thì mở luôn repo vừa tải về.
    QObject::connect(&cloner, &gitapp::CloneController::finished, &repo, &gitapp::RepoController::openRepo);

    QQmlApplicationEngine engine;
    gitapp::LanguageController language(!headless);
    language.setRetranslateCallback([&engine] { engine.retranslate(); });
    const QString requestedLanguage = optionValue(args, QStringLiteral("--language"));
    if (headless)
        language.applyLanguage(requestedLanguage.isEmpty() ? QStringLiteral("vi") : requestedLanguage);
    else
        language.applySavedLanguage();
    engine.rootContext()->setContextProperty(QStringLiteral("repo"), &repo);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("cloner"), &cloner);
    engine.rootContext()->setContextProperty(QStringLiteral("languageController"), &language);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/GitGui/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    QObject *root = engine.rootObjects().first();

    if (headless) {
        const QString repoArg = smoke ? (smokeIdx + 1 < args.size() ? args.at(smokeIdx + 1) : QString())
                                      : (shotIdx + 2 < args.size() && !args.at(shotIdx + 2).startsWith(QLatin1String("--"))
                                             ? args.at(shotIdx + 2) : QString());
        if (!repoArg.isEmpty())
            repo.openRepo(repoArg);
        const QString tab = optionValue(args, QStringLiteral("--tab"));
        if (!tab.isEmpty())
            QTimer::singleShot(500, &app, [root, tab] { QMetaObject::invokeMethod(root, "setTab", Q_ARG(QVariant, tab.toInt())); });

        // `--toggle-diff-row N`: bấm vào hàng N của diff (kiểm tra giao diện khi loại dòng khỏi commit).
        const QString toggleRow = optionValue(args, QStringLiteral("--toggle-diff-row"));
        if (!toggleRow.isEmpty())
            QTimer::singleShot(1300, &repo, [&repo, toggleRow] { repo.toggleDiffRow(toggleRow.toInt()); });
        const QString dialog = optionValue(args, QStringLiteral("--dialog"));
        if (!dialog.isEmpty())
            QTimer::singleShot(900, &app, [root, dialog] { QMetaObject::invokeMethod(root, "openDialog", Q_ARG(QVariant, dialog)); });

        if (smoke) {
            // Sau khi repo nạp xong, mở mọi hộp thoại để các binding của chúng được thực thi.
            QTimer::singleShot(800, &app, [root] { QMetaObject::invokeMethod(root, "smokeOpenDialogs"); });
            QTimer::singleShot(2000, &app, &QCoreApplication::quit);
            const int rc = app.exec();
            return rc != 0 ? rc : (g_qmlWarning ? 2 : 0);
        }

        const QString out = args.value(shotIdx + 1);
        QTimer::singleShot(1800, &app, [root, out] {
            if (auto *w = qobject_cast<QQuickWindow *>(root))
                w->grabWindow().save(out);
            QCoreApplication::quit();
        });
        return app.exec();
    }

    // Mở lại repo gần nhất khi khởi động.
    const QStringList recent = repo.recent()->paths();
    if (!recent.isEmpty())
        repo.openRepo(recent.first());

    // Quay lại app (vd sau khi sửa file conflict bằng editor ngoài): làm mới trạng thái.
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &repo,
                     [&repo](Qt::ApplicationState state) {
                         if (state == Qt::ApplicationActive && repo.hasRepo() && !repo.busy()) {
                             repo.refresh();
                             repo.conflictController()->reloadIfChanged();
                         }
                     });

    return app.exec();
}
