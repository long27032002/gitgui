#pragma once

#include <QObject>
#include <QTranslator>

#include <functional>

namespace gitapp {

class LanguageController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)

public:
    explicit LanguageController(bool persistSettings, QObject *parent = nullptr);
    ~LanguageController() override;

    [[nodiscard]] QString language() const { return m_language; }
    void setRetranslateCallback(std::function<void()> callback) { m_retranslate = std::move(callback); }
    void applySavedLanguage();
    void applyLanguage(const QString &code);

    Q_INVOKABLE void setLanguage(const QString &code);

signals:
    void languageChanged();

private:
    void setLanguageInternal(const QString &code, bool persist);

    QTranslator m_translator;
    std::function<void()> m_retranslate;
    QString m_language = QStringLiteral("vi");
    bool m_translatorInstalled = false;
    bool m_persistSettings = true;
};

} // namespace gitapp
