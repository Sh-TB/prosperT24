// diagnostics/collectors/prx_collector.hpp — PRX module diagnostics collector
//
// Tracks PRX (PlayStation Relocatable eXecutable) loading, dependencies,
// initialization functions, and export/import tables.
#pragma once

#include "collector.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace prosper {
namespace diagnostics {

struct PrxModuleInfo {
    std::string     path;           // File path
    std::string     name;           // Module name (basename)
    uint64_t        load_address = 0;  // Where it was mapped
    uint64_t        size = 0;       // Image size in memory
    bool            loaded = false;
    bool            init_called = false;
    bool            init_success = false;
    
    // Dependencies
    std::vector<std::string> dependencies;
    std::vector<std::string> dependents;  // Who depends on this
    
    // Export table summary
    size_t          export_count = 0;
    size_t          import_count = 0;
    
    // Initialization
    std::vector<uint64_t> init_functions;  // Init array addresses
    
    // Timing
    uint64_t        load_time_ms = 0;
    uint64_t        init_time_ms = 0;
    
    // Status
    std::string     error_message;
};

struct ImportInfo {
    std::string     nid;             // NID (Name ID) hash
    std::string     name;            // Resolved function name (if known)
    std::string     owner_module;    // Which module provides this import
    bool            resolved = false;
    bool            stubbed = false;   // Resolved to HLE stub
    uint64_t        call_count = 0;    // How many times called
    uint64_t        address = 0;       // Import stub address
};

class PrxCollector : public Collector {
public:
    PrxCollector() = default;
    
    const char* name() const override { return "prx"; }
    Subsystem subsystem() const override { return Subsystem::LOADER; }
    
    bool initialize() override {
        modules_.clear();
        imports_.clear();
        return true;
    }
    
    // --- PRX Recording Interface ---------------------------------------------
    
    // Called when a PRX module starts loading
    void record_prx_load_start(const std::string& path, uint64_t base_addr);
    
    // Called when a PRX module finishes loading
    void record_prx_loaded(const std::string& path, uint64_t size, 
                           bool success, const std::string& error = "");
    
    // Called when init_array functions are about to run
    void record_prx_init_start(const std::string& path);
    
    // Called after init_array completes
    void record_prx_init_complete(const std::string& path, bool success,
                                  uint64_t time_ms);
    
    // Set module dependencies
    void set_dependencies(const std::string& path, 
                          const std::vector<std::string>& deps);
    
    // Set export/import counts
    void set_module_counts(const std::string& path,
                           size_t exports, size_t imports);
    
    // --- Import Recording Interface ------------------------------------------
    
    // Record an import resolution
    void record_import_resolved(const std::string& nid, const std::string& name,
                                const std::string& owner, bool stubbed,
                                uint64_t address);
    
    // Record an unresolved/missing import
    void record_import_missing(const std::string& nid, uint64_t address);
    
    // Record that an import was called (for frequency tracking)
    void record_import_called(const std::string& nid);
    
    // --- Query Interface ----------------------------------------------------
    
    const std::vector<PrxModuleInfo>& modules() const { return modules_; }
    const std::vector<ImportInfo>& imports() const { return imports_; }
    
    // Get missing imports (for failure analysis)
    std::vector<const ImportInfo*> get_missing_imports() const;
    
    // Get most-called stubbed imports (potential HLE gaps)
    std::vector<const ImportInfo*> get_frequent_stubs(size_t min_calls = 10) const;
    
    // Generate JSON report
    std::string generate_report() const override;

private:
    PrxModuleInfo* find_or_create_module(const std::string& path);
    ImportInfo* find_import_by_nid(const std::string& nid);
    
    std::vector<PrxModuleInfo> modules_;
    std::vector<ImportInfo> imports_;
    
    std::string module_to_json(const PrxModuleInfo& m) const;
    std::string import_to_json(const ImportInfo& i) const;
};

} // namespace diagnostics
} // namespace prosper
