#pragma once
// Forces the linker to pull in the static-library resource objects
// (built-in themes). Call once during startup before reading ":/themes/...".
namespace cf {

void initBuiltinResources();

}  // namespace cf
