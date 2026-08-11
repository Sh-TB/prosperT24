// diagnostics/collectors/collector.cpp — Out-of-line definitions for Collector base class
//
// The virtual destructor must be defined in exactly one translation unit
// to avoid linker errors when the header is included from multiple .cpp files.
#include "collector.hpp"

namespace prosper {
namespace diagnostics {

// Out-of-line virtual destructor definition
Collector::~Collector() = default;

} // namespace diagnostics
} // namespace prosper
