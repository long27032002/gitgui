#include "LanguageController.h"

#include <QCoreApplication>
#include <QSettings>

namespace gitapp {

LanguageController::LanguageController(bool persistSettings, QObject *parent)
    : QObject(parent), m_persistSettings(persistSettings)
{
}

LanguageController::~LanguageController()
{
    if (m_translatorInstalled)
        QCoreApplication::removeTranslator(&m_translator);
}

void LanguageController::applySavedLanguage()
{
    QSettings settings;
    setLanguageInternal(settings.value(QStringLiteral("language/code"), QStringLiteral("vi")).toString(),
                        false);
}

void LanguageController::applyLanguage(const QString &code)
{
    setLanguageInternal(code, false);
}

void LanguageController::setLanguage(const QString &code)
{
    setLanguageInternal(code, m_persistSettings);
}

void LanguageController::setLanguageInternal(const QString &code, bool persist)
{
    if (code != QLatin1String("vi") && code != QLatin1String("en"))
        return;

    if (code == QLatin1String("en") && !m_translatorInstalled) {
        if (!m_translator.load(QStringLiteral(":/i18n/gitgui_en.qm")))
            return;
        QCoreApplication::installTranslator(&m_translator);
        m_translatorInstalled = true;
    } else if (code == QLatin1String("vi") && m_translatorInstalled) {
        QCoreApplication::removeTranslator(&m_translator);
        m_translatorInstalled = false;
    }

    const bool changed = m_language != code;
    m_language = code;
    if (persist) {
        QSettings settings;
        settings.setValue(QStringLiteral("language/code"), code);
    }
    if (m_retranslate)
        m_retranslate();
    if (changed)
        emit languageChanged();
}

} // namespace gitapp
