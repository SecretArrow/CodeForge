#include "lsp/LanguageService.h"

#include <QHash>

namespace cf {

LanguageServiceRegistry& LanguageServiceRegistry::instance()
{
    static LanguageServiceRegistry reg;
    return reg;
}

void LanguageServiceRegistry::registerService(const QString& languageId, LanguageService* svc)
{
    if (svc) m_services.insert(languageId, svc);
    else m_services.remove(languageId);
}

LanguageService* LanguageServiceRegistry::serviceFor(const QString& languageId)
{
    return m_services.value(languageId, &m_null);
}

}  // namespace cf
