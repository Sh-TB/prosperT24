// diagnostics/collectors/prx_collector.cpp — PRX collector implementation
#include "prx_collector.hpp"
#include <algorithm>
#include <sstream>

namespace prosper {
namespace diagnostics {

PrxModuleInfo* PrxCollector::find_or_create_module(const std::string& path) {
    for (auto& m : modules_) {
        if (m.path == path) return &m;
    }
    PrxModuleInfo info;
    info.path = path;
    // Extract basename
    auto slash = path.find_last_of("/\\");
    info.name = (slash != std::string::npos) ? path.substr(slash + 1) : path;
    modules_.push_back(info);
    return &modules_.back();
}

ImportInfo* PrxCollector::find_import_by_nid(const std::string& nid) {
    for (auto& i : imports_) {
        if (i.nid == nid) return &i;
    }
    return nullptr;
}

void PrxCollector::record_prx_load_start(const std::string& path, uint64_t base_addr) {
    auto* mod = find_or_create_module(path);
    mod->load_address = base_addr;
    
    // Build message with proper hex formatting
    std::ostringstream msg_ss;
    msg_ss << "Loading PRX: " << mod->name << " @ 0x" << std::hex << base_addr;
    
    emit_event(
        "PRX_LOAD_START",
        Severity::INFO,
        msg_ss.str(),
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& e) {
            e.add_string("path", path);
            e.add_string("name", mod->name);
            e.add_string("base_address", "0x" + std::to_string(base_addr));
        }
    );
}

void PrxCollector::record_prx_loaded(const std::string& path, uint64_t size,
                                     bool success, const std::string& error) {
    auto* mod = find_or_create_module(path);
    mod->size = size;
    mod->loaded = success;
    mod->error_message = error;
    
    emit_event(
        "PRX_LOADED",
        success ? Severity::INFO : Severity::ERROR,
        success 
            ? ("PRX loaded: " + mod->name + " (" + std::to_string(size) + " bytes)")
            : ("PRX load FAILED: " + mod->name + " - " + error),
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& e) {
            e.add_string("path", path);
            e.add_string("name", mod->name);
            e.add_uint("size", size);
            e.add_bool("success", success);
            if (!error.empty()) e.add_string("error", error);
        }
    );
}

void PrxCollector::record_prx_init_start(const std::string& path) {
    auto* mod = find_or_create_module(path);
    
    emit_event(
        "PRX_INIT_START",
        Severity::INFO,
        "Running init_array for: " + mod->name,
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& e) {
            e.add_string("path", path);
            e.add_string("name", mod->name);
            e.add_uint("init_count", mod->init_functions.size());
        }
    );
}

void PrxCollector::record_prx_init_complete(const std::string& path, bool success,
                                             uint64_t time_ms) {
    auto* mod = find_or_create_module(path);
    mod->init_called = true;
    mod->init_success = success;
    mod->init_time_ms = time_ms;
    
    emit_event(
        success ? "PRX_INIT_COMPLETE" : "PRX_INIT_FAILED",
        success ? Severity::INFO : Severity::ERROR,
        success
            ? ("Init complete: " + mod->name + " in " + std::to_string(time_ms) + "ms")
            : ("Init FAILED: " + mod->name),
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& e) {
            e.add_string("path", path);
            e.add_string("name", mod->name);
            e.add_bool("success", success);
            e.add_uint("time_ms", time_ms);
        }
    );
}

void PrxCollector::set_dependencies(const std::string& path,
                                     const std::vector<std::string>& deps) {
    auto* mod = find_or_create_module(path);
    mod->dependencies = deps;
    
    // Register reverse dependencies
    for (const auto& dep : deps) {
        auto* dep_mod = find_or_create_module(dep);
        dep_mod->dependents.push_back(path);
    }
}

void PrxCollector::set_module_counts(const std::string& path,
                                      size_t exports, size_t imports) {
    auto* mod = find_or_create_module(path);
    mod->export_count = exports;
    mod->import_count = imports;
}

void PrxCollector::record_import_resolved(const std::string& nid,
                                          const std::string& name,
                                          const std::string& owner,
                                          bool stubbed,
                                          uint64_t address) {
    ImportInfo info;
    info.nid = nid;
    info.name = name;
    info.owner_module = owner;
    info.resolved = true;
    info.stubbed = stubbed;
    info.address = address;
    
    // Check if we already have this import
    auto* existing = find_import_by_nid(nid);
    if (existing) {
        *existing = info;
    } else {
        imports_.push_back(info);
    }
    
    emit_event(
        stubbed ? "IMPORT_STUBBED" : "IMPORT_RESOLVED",
        stubbed ? Severity::WARNING : Severity::DEBUG,
        stubbed
            ? ("Import stubbed: " + name + " [" + nid + "]")
            : ("Import resolved: " + name + " -> " + owner),
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& e) {
            e.add_string("nid", nid);
            e.add_string("name", name);
            e.add_string("owner", owner);
            e.add_bool("stubbed", stubbed);
            e.add_string("address", "0x" + std::to_string(address));
        }
    );
}

void PrxCollector::record_import_missing(const std::string& nid, uint64_t address) {
    ImportInfo info;
    info.nid = nid;
    info.resolved = false;
    info.stubbed = false;
    info.address = address;
    
    auto* existing = find_import_by_nid(nid);
    if (existing) {
        existing->resolved = false;  // Mark as missing even if previously found
    } else {
        imports_.push_back(info);
    }
    
    emit_event(
        "IMPORT_MISSING",
        Severity::WARNING,
        "Missing import: " + nid,
        SourceLocation(__FILE__, __LINE__, __func__),
        [=](DiagnosticEvent& e) {
            e.add_string("nid", nid);
            e.add_string("address", "0x" + std::to_string(address));
        }
    );
}

void PrxCollector::record_import_called(const std::string& nid) {
    auto* imp = find_import_by_nid(nid);
    if (imp) {
        imp->call_count++;
    }
    // Don't emit event for every call - too noisy
}

std::vector<const ImportInfo*> PrxCollector::get_missing_imports() const {
    std::vector<const ImportInfo*> result;
    for (const auto& i : imports_) {
        if (!i.resolved) result.push_back(&i);
    }
    return result;
}

std::vector<const ImportInfo*> PrxCollector::get_frequent_stubs(size_t min_calls) const {
    std::vector<const ImportInfo*> result;
    for (const auto& i : imports_) {
        if (i.stubbed && i.call_count >= min_calls) {
            result.push_back(&i);
        }
    }
    // Sort by call count descending
    std::sort(result.begin(), result.end(),
              [](const ImportInfo* a, const ImportInfo* b) {
                  return a->call_count > b->call_count;
              });
    return result;
}

std::string PrxCollector::module_to_json(const PrxModuleInfo& m) const {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "      \"path\": \"" << m.path << "\",\n";
    ss << "      \"name\": \"" << m.name << "\",\n";
    ss << "      \"load_address\": \"0x" << std::hex << m.load_address << "\",\n";
    ss << std::dec;
    ss << "      \"size\": " << m.size << ",\n";
    ss << "      \"loaded\": " << (m.loaded ? "true" : "false") << ",\n";
    ss << "      \"init_called\": " << (m.init_called ? "true" : "false") << ",\n";
    ss << "      \"init_success\": " << (m.init_success ? "true" : "false") << ",\n";
    ss << "      \"exports\": " << m.export_count << ",\n";
    ss << "      \"imports\": " << m.import_count << ",\n";
    ss << "      \"load_time_ms\": " << m.load_time_ms << ",\n";
    ss << "      \"init_time_ms\": " << m.init_time_ms << ",\n";
    
    // Dependencies
    ss << "      \"dependencies\": [";
    for (size_t i = 0; i < m.dependencies.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << m.dependencies[i] << "\"";
    }
    ss << "],\n";
    
    if (!m.error_message.empty()) {
        ss << "      \"error\": \"" << m.error_message << "\",\n";
    }
    
    ss << "    }";
    return ss.str();
}

std::string PrxCollector::import_to_json(const ImportInfo& i) const {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "      \"nid\": \"" << i.nid << "\",\n";
    ss << "      \"name\": \"" << i.name << "\",\n";
    ss << "      \"owner\": \"" << i.owner_module << "\",\n";
    ss << "      \"status\": ";
    if (!i.resolved) ss << "\"MISSING\"";
    else if (i.stubbed) ss << "\"STUB\"";
    else ss << "\"RESOLVED\"";
    ss << ",\n";
    ss << "      \"calls\": " << i.call_count << ",\n";
    ss << "      \"address\": \"0x" << std::hex << i.address << "\"\n";
    ss << std::dec;
    ss << "    }";
    
    return ss.str();
}

std::string PrxCollector::generate_report() const {
    std::ostringstream ss;
    
    ss << "{\n";
    ss << "  \"total_modules\": " << modules_.size() << ",\n";
    ss << "  \"loaded_modules\": " << 
        std::count_if(modules_.begin(), modules_.end(),
                      [](const PrxModuleInfo& m) { return m.loaded; }) << ",\n";
    ss << "  \"failed_modules\": " <<
        std::count_if(modules_.begin(), modules_.end(),
                      [](const PrxModuleInfo& m) { return !m.loaded; }) << ",\n";
    ss << "  \"total_imports\": " << imports_.size() << ",\n";
    ss << "  \"resolved_imports\": " <<
        std::count_if(imports_.begin(), imports_.end(),
                      [](const ImportInfo& i) { return i.resolved && !i.stubbed; }) << ",\n";
    ss << "  \"stubbed_imports\": " <<
        std::count_if(imports_.begin(), imports_.end(),
                      [](const ImportInfo& i) { return i.stubbed; }) << ",\n";
    ss << "  \"missing_imports\": " <<
        std::count_if(imports_.begin(), imports_.end(),
                      [](const ImportInfo& i) { return !i.resolved; }) << ",\n";
    
    // Modules array
    ss << "  \"modules\": [\n";
    for (size_t i = 0; i < modules_.size(); ++i) {
        ss << "    " << module_to_json(modules_[i]);
        if (i < modules_.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    // Imports array (summary - only missing and frequently-called stubs)
    auto missing = get_missing_imports();
    auto frequent_stubs = get_frequent_stubs(5);  // Called 5+ times
    
    ss << "  \"missing_imports\": [\n";
    for (size_t i = 0; i < missing.size(); ++i) {
        ss << "    " << import_to_json(*missing[i]);
        if (i < missing.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    ss << "  \"frequent_stubs\": [\n";
    for (size_t i = 0; i < frequent_stubs.size(); ++i) {
        ss << "    " << import_to_json(*frequent_stubs[i]);
        if (i < frequent_stubs.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    
    ss << "}\n";
    
    return ss.str();
}

} // namespace diagnostics
} // namespace prosper
