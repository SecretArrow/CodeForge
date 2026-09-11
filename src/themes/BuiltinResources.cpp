#include "themes/BuiltinResources.h"

#include <QtCore/qtsymbolmacros.h>

// Keeps the AUTORCC-generated resource init object (resources.qrc) linked
// from the static library. Global scope so the extern symbol is not namespaced.
QT_DECLARE_EXTERN_RESOURCE(resources)

namespace {

struct ResourceKeeper {
    ResourceKeeper() { QT_KEEP_RESOURCE(resources) }
};
static ResourceKeeper resourceKeeperInstance;

}  // namespace

void cf::initBuiltinResources()
{
    // The static initializer above performs registration; nothing to do here
    // beyond providing a link-time reference to this translation unit.
}
