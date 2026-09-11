#pragma once
// Plugin/extension architecture.
//
// v1 scope (per project spec): the loading architecture is real — native Qt
// plugins implementing IExtension can be dropped into the extensions folder
// and are loaded at startup via QPluginLoader. Data-driven themes/languages
// are the first extension surfaces.
#include <QtPlugin>

class QWidget;

namespace cf {

struct AppContext;

class IExtension {
public:
    virtual ~IExtension() = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QString description() const = 0;

    // Called once after successful load. Return false to unload.
    virtual bool initialize(cf::AppContext& context) = 0;
};

}  // namespace cf

Q_DECLARE_INTERFACE(cf::IExtension, "org.codeforge.IExtension/1.0")
