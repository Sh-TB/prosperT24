#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <algorithm>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Import Resolution Plugin - Phase 9.5 Diagnostic Plugin
//
// Tracks NID/import resolution for PS4 emulator diagnostics.
// Records each import resolution attempt (success/failure), tracks NID to
// function name mapping, records caller addresses, and classifies imports
// by status for risk assessment.
//=============================================================================

/**
 * @brief Represents a single import entry with full tracking data
 */
struct ImportEntry {
    std::string nid;                        ///< NID (Name ID) - unique identifier for the import
    std::string name;                       ///< Human-readable function/symbol name
    std::string module;                     ///< Module this import belongs to
    ImportStatus status{ImportStatus::MISSING_NOT_CALLED};  ///< Current status of this import
    uint64_t caller_addr{0};                ///< Address of code calling this import
    size_t call_count{0};                   ///< Number of times this import was called
    Timestamp first_call;                   ///< Timestamp of first call attempt
    Timestamp last_call;                    ///< Timestamp of most recent call
    uint64_t resolved_addr{0};              ///< Address where import was resolved to
    
    /// Default constructor
    ImportEntry() = default;
    
    /// Constructor with essential fields
    ImportEntry(const std::string& n, const std::string& nm, const std::string& mod)
        : nid(n), name(nm), module(mod) {}
    
    /// Check if this import is considered "working"
    bool is_working() const {
        return status == ImportStatus::CALLED_SUCCESSFULLY || 
               status == ImportStatus::STUB_IMPLEMENTED;
    }
    
    /// Check if this import represents a potential crash risk
    bool is_high_risk() const {
        return status == ImportStatus::MISSING_CALLED;
    }
    
    /// Get status as human-readable string
    std::string status_string() const {
        switch (status) {
            case ImportStatus::MISSING_NOT_CALLED: return "MISSING_NOT_CALLED";
            case ImportStatus::MISSING_CALLED: return "MISSING_CALLED";
            case ImportStatus::IMPLEMENTED_BUT_FAILED: return "IMPLEMENTED_BUT_FAILED";
            case ImportStatus::CALLED_SUCCESSFULLY: return "CALLED_SUCCESSFULLY";
            case ImportStatus::STUB_IMPLEMENTED: return "STUB_IMPLEMENTED";
            default: return "UNKNOWN";
        }
    }
    
    /// Serialize to JSON string
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"nid\":\"" << nid << "\",";
        oss << "\"name\":\"" << name << "\",";
        oss << "\"module\":\"" << module << "\",";
        oss << "\"status\":" << static_cast<int>(status) << ",";
        oss << "\"status_str\":\"" << status_string() << "\",";
        oss << "\"caller_addr\":\"0x" << std::hex << caller_addr << std::dec << "\",";
        oss << "\"resolved_addr\":\"0x" << std::hex << resolved_addr << std::dec << "\",";
        oss << "\"call_count\":" << call_count << ",";
        
        if (first_call.time_since_epoch().count() != 0) {
            oss << "\"first_call_ms\":" << std::fixed << std::setprecision(3)
                << timestamp_to_ms(first_call) << ",";
        } else {
            oss << "\"first_call_ms\":null,";
        }
        
        if (last_call.time_since_epoch().count() != 0) {
            oss << "\"last_call_ms\":" << std::fixed << std::setprecision(3)
                << timestamp_to_ms(last_call);
        } else {
            oss << "\"last_call_ms\":null";
        }
        
        oss << "}";
        return oss.str();
    }
};

/**
 * @brief Summary statistics for import analysis
 */
struct ImportStatistics {
    size_t total_imports{0};
    size_t resolved_imports{0};
    size_t missing_not_called{0};
    size_t missing_called{0};           // HIGH RISK
    size_t implemented_but_failed{0};
    size_t called_successfully{0};
    size_t stub_implemented{0};
    
    double coverage_percent() const {
        return total_imports > 0 
            ? (static_cast<double>(called_successfully + stub_implemented) / total_imports) * 100.0
            : 0.0;
    }
    
    double risk_percent() const {
        return total_imports > 0 
            ? (static_cast<double>(missing_called) / total_imports) * 100.0
            : 0.0;
    }
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"total_imports\":" << total_imports << ",";
        oss << "\"resolved_imports\":" << resolved_imports << ",";
        oss << "\"missing_not_called\":" << missing_not_called << ",";
        oss << "\"missing_called\":" << missing_called << ",";
        oss << "\"implemented_but_failed\":" << implemented_but_failed << ",";
        oss << "\"called_successfully\":" << called_successfully << ",";
        oss << "\"stub_implemented\":" << stub_implemented << ",";
        oss << "\"coverage_percent\":" << std::fixed << std::setprecision(2) << coverage_percent() << ",";
        oss << "\"risk_percent\":" << std::fixed << std::setprecision(2) << risk_percent();
        oss << "}";
        return oss.str();
    }
};

/**
 * @brief Import Resolution Plugin implementation
 * 
 * This plugin provides comprehensive import/NID tracking:
 * - Records each import resolution attempt with success/failure status
 * - Maintains NID to function name mapping
 * - Records caller address for each import
 * - Classifies imports by status for risk assessment
 * - Generates detailed reports on import health
 */
class ImportResolutionPlugin : public DiagnosticPlugin {
public:
    //=========================================================================
    // Constructor / Destructor
    //=========================================================================
    
    ImportResolutionPlugin()
        : max_events_(10000),
          track_callers_(true) {}
    
    virtual ~ImportResolutionPlugin() override {
        shutdown();
    }
    
    //=========================================================================
    // DiagnosticPlugin Interface Implementation
    //=========================================================================
    
    std::string name() const override { return "ImportResolution"; }
    
    std::string version() const override { return "1.5.0"; }
    
    std::string description() const override {
        return "Tracks NID/import resolution, status classification, and risk assessment";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        imports_.clear();
        nid_index_.clear();
        name_index_.clear();
        module_index_.clear();
        
        active_ = true;
        
        // Parse configuration
        auto it = config_.find("max_events");
        if (it != config_.end()) {
            try { max_events_ = static_cast<size_t>(std::stoull(it->second)); } catch (...) {}
        }
        
        it = config_.find("track_callers");
        if (it != config_.end()) {
            track_callers_ = (it->second == "true" || it->second == "1");
        }
        
        emit_info("ImportResolution initialized successfully");
        return true;
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
        emit_info("ImportResolution shut down");
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        imports_.clear();
        nid_index_.clear();
        name_index_.clear();
        module_index_.clear();
        event_count_ = 0;
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        if (event_count_ >= max_events_) return;
        
        // Process import-related events
        if (event.event_type == "import_resolved") {
            auto nid_it = event.metadata.find("nid");
            auto name_it = event.metadata.find("name");
            auto mod_it = event.metadata.find("module");
            
            if (nid_it != event.metadata.end()) {
                uint64_t resolved_addr = 0;
                uint64_t caller_addr = 0;
                
                auto addr_it = event.numeric_data.find("resolved_addr");
                if (addr_it != event.numeric_data.end()) {
                    resolved_addr = static_cast<uint64_t>(addr_it->second);
                }
                
                auto caller_it = event.numeric_data.find("caller_addr");
                if (caller_it != event.numeric_data.end()) {
                    caller_addr = static_cast<uint64_t>(caller_it->second);
                }
                
                on_import_resolved_internal(nid_it->second,
                    name_it != event.metadata.end() ? name_it->second : "",
                    mod_it != event.metadata.end() ? mod_it->second : "",
                    resolved_addr, caller_addr, event.timestamp);
            }
        }
        else if (event.event_type == "import_failed") {
            auto nid_it = event.metadata.find("nid");
            if (nid_it != event.metadata.end()) {
                uint64_t caller_addr = 0;
                
                auto caller_it = event.numeric_data.find("caller_addr");
                if (caller_it != event.numeric_data.end()) {
                    caller_addr = static_cast<uint64_t>(caller_it->second);
                }
                
                on_import_failed_internal(nid_it->second,
                    event.metadata.count("name") ? event.metadata.at("name") : "",
                    event.metadata.count("module") ? event.metadata.at("module") : "",
                    caller_addr, event.timestamp);
            }
        }
        else if (event.event_type == "import_called") {
            auto nid_it = event.metadata.find("nid");
            if (nid_it != event.metadata.end()) {
                bool success = true;
                auto success_it = event.numeric_data.find("success");
                if (success_it != event.numeric_data.end()) {
                    success = success_it->second != 0;
                }
                
                update_import_call_internal(nid_it->second, success, event.timestamp);
            }
        }
        
        event_count_++;
    }
    
    std::string generate_report() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return generate_report_internal();
    }
    
    void export_json(const std::string& path) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ofstream file(path);
        if (!file.is_open()) return;
        
        file << generate_report_internal();
        file.close();
    }
    
    //=========================================================================
    // Public API Methods
    //=========================================================================
    
    /**
     * @brief Record that an import was successfully resolved
     * @param nid NID of the import
     * @param name Function/symbol name
     * @param module Module containing this import
     * @param resolved_addr Address where import was resolved
     * @param caller_addr Address of code requesting this import
     * @param timestamp Optional timestamp (defaults to now)
     */
    void on_import_resolved(const std::string& nid,
                            const std::string& name,
                            const std::string& module,
                            uint64_t resolved_addr,
                            uint64_t caller_addr = 0,
                            Timestamp timestamp = Timestamp{}) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_import_resolved_internal(nid, name, module, resolved_addr, caller_addr,
                                    timestamp.time_since_epoch().count() == 0 ? now() : timestamp);
    }
    
    /**
     * @brief Record that an import failed to resolve
     * @param nid NID of the failed import
     * @param name Function/symbol name (may be unknown)
     * @param module Module containing this import
     * @param caller_addr Address of code requesting this import
     * @param timestamp Optional timestamp (defaults to now)
     */
    void on_import_failed(const std::string& nid,
                          const std::string& name,
                          const std::string& module,
                          uint64_t caller_addr = 0,
                          Timestamp timestamp = Timestamp{}) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_import_failed_internal(nid, name, module, caller_addr,
                                  timestamp.time_since_epoch().count() == 0 ? now() : timestamp);
    }
    
    /**
     * @brief Get the current status of a specific import by NID
     * @param nid NID to look up
     * @return ImportStatus of the import, or MISSING_NOT_CALLED if not found
     */
    ImportStatus get_import_status(const std::string& nid) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = nid_index_.find(nid);
        if (it != nid_index_.end()) {
            return imports_[it->second].status;
        }
        return ImportStatus::MISSING_NOT_CALLED;
    }
    
    /**
     * @brief Get complete information about an import by NID
     * @param nid NID to look up
     * @return Pointer to ImportEntry or nullptr if not found
     */
    const ImportEntry* get_import_info(const std::string& nid) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = nid_index_.find(nid);
        if (it != nid_index_.end()) {
            return &imports_[it->second];
        }
        return nullptr;
    }
    
    /**
     * @brief Get all imports that are missing (not implemented)
     * @return Vector of pointers to missing imports
     */
    std::vector<const ImportEntry*> get_missing_imports() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<const ImportEntry*> result;
        for (const auto& imp : imports_) {
            if (imp.status == ImportStatus::MISSING_NOT_CALLED ||
                imp.status == ImportStatus::MISSING_CALLED) {
                result.push_back(&imp);
            }
        }
        return result;
    }
    
    /**
     * @brief Get all high-risk imports (missing but being called)
     * @return Vector of pointers to high-risk imports
     */
    std::vector<const ImportEntry*> get_high_risk_imports() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<const ImportEntry*> result;
        for (const auto& imp : imports_) {
            if (imp.status == ImportStatus::MISSING_CALLED) {
                result.push_back(&imp);
            }
        }
        return result;
    }
    
    /**
     * @brief Get imports belonging to a specific module
     * @param module_name Module to filter by
     * @return Vector of pointers to imports in that module
     */
    std::vector<const ImportEntry*> get_module_imports(const std::string& module_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<const ImportEntry*> result;
        auto range = module_index_.equal_range(module_name);
        for (auto it = range.first; it != range.second; ++it) {
            result.push_back(&imports_[it->second]);
        }
        return result;
    }
    
    /**
     * @brief Get overall import statistics
     */
    ImportStatistics get_statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return calculate_statistics();
    }
    
    /**
     * @brief Record that an import was called and its result
     * @param nid NID of the called import
     * @param success Whether the call succeeded
     * @param timestamp Optional timestamp (defaults to now)
     */
    void record_import_call(const std::string& nid, bool success, Timestamp timestamp = Timestamp{}) {
        std::lock_guard<std::mutex> lock(mutex_);
        update_import_call_internal(nid, success,
                                    timestamp.time_since_epoch().count() == 0 ? now() : timestamp);
    }
    
    /**
     * @brief Set or update the function name for an NID
     * @param nid NID to update
     * @param name New function name
     */
    void set_import_name(const std::string& nid, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = nid_index_.find(nid);
        if (it != nid_index_.end()) {
            // Update name index - remove old entry if name changed
            if (!imports_[it->second].name.empty()) {
                auto name_range = name_index_.equal_range(imports_[it->second].name);
                for (auto nit = name_range.first; nit != name_range.second; ++nit) {
                    if (nit->second == it->second) {
                        name_index_.erase(nit);
                        break;
                    }
                }
            }
            
            imports_[it->second].name = name;
            if (!name.empty()) {
                name_index_.insert({name, it->second});
            }
        }
    }
    
    /**
     * @brief Look up an import by function name
     * @param name Function name to search for
     * @return Pointer to first matching ImportEntry or nullptr
     */
    const ImportEntry* find_by_name(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = name_index_.find(name);
        if (it != name_index_.end()) {
            return &imports_[it->second];
        }
        return nullptr;
    }
    
    /**
     * @brief Get total number of tracked imports
     */
    size_t import_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return imports_.size();
    }

private:
    //=========================================================================
    // Internal Implementation Methods
    //=========================================================================
    
    /**
     * @brief Internal import resolution handler (assumes lock held)
     */
    void on_import_resolved_internal(const std::string& nid,
                                     const std::string& name,
                                     const std::string& module,
                                     uint64_t resolved_addr,
                                     uint64_t caller_addr,
                                     Timestamp timestamp) {
        // Check for existing entry
        auto it = nid_index_.find(nid);
        if (it != nid_index_.end()) {
            // Update existing entry
            auto& entry = imports_[it->second];
            entry.resolved_addr = resolved_addr;
            
            if (track_callers_ && caller_addr != 0) {
                entry.caller_addr = caller_addr;
            }
            
            // Update status based on previous state
            if (entry.status == ImportStatus::MISSING_NOT_CALLED ||
                entry.status == ImportStatus::MISSING_CALLED) {
                entry.status = ImportStatus::STUB_IMPLEMENTED;
            }
            
            emit_info("Import re-resolved: " + nid + " (" + name + ")");
            return;
        }
        
        // Create new entry
        ImportEntry entry(nid, name.empty() ? "unknown_" + nid : name, module);
        entry.status = ImportStatus::STUB_IMPLEMENTED;
        entry.resolved_addr = resolved_addr;
        entry.caller_addr = track_callers_ ? caller_addr : 0;
        entry.first_call = timestamp;
        entry.last_call = timestamp;
        
        size_t index = imports_.size();
        imports_.push_back(std::move(entry));
        nid_index_[nid] = index;
        
        if (!name.empty()) {
            name_index_.insert({name, index});
        }
        if (!module.empty()) {
            module_index_.insert({module, index});
        }
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = DiagnosticPlugin::name();
        event.event_type = "import_tracked";
        event.severity = Severity::INFO;
        event.message = "Import resolved: " + nid + " -> " + name;
        event.metadata["nid"] = nid;
        event.metadata["function_name"] = name;
        event.metadata["module"] = module;
        event.numeric_data["resolved_addr"] = static_cast<int64_t>(resolved_addr);
        event.numeric_data["total_imports"] = static_cast<int64_t>(imports_.size());
        
        emit_event(event);
    }
    
    /**
     * @brief Internal import failure handler (assumes lock held)
     */
    void on_import_failed_internal(const std::string& nid,
                                   const std::string& name,
                                   const std::string& module,
                                   uint64_t caller_addr,
                                   Timestamp timestamp) {
        // Check for existing entry
        auto it = nid_index_.find(nid);
        if (it != nid_index_.end()) {
            auto& entry = imports_[it->second];
            
            if (entry.call_count > 0) {
                entry.status = ImportStatus::MISSING_CALLED;
            }
            
            emit_warning("Import failed (already tracked): " + nid);
            return;
        }
        
        // Create new entry as missing
        ImportEntry entry(nid, name.empty() ? "unknown_" + nid : name, module);
        entry.status = ImportStatus::MISSING_NOT_CALLED;
        entry.caller_addr = track_callers_ ? caller_addr : 0;
        entry.first_call = timestamp;
        
        size_t index = imports_.size();
        imports_.push_back(std::move(entry));
        nid_index_[nid] = index;
        
        if (!name.empty()) {
            name_index_.insert({name, index});
        }
        if (!module.empty()) {
            module_index_.insert({module, index});
        }
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = DiagnosticPlugin::name();
        event.event_type = "import_missing";
        event.severity = Severity::WARNING;
        event.message = "Import not found: " + nid + " (" + name + ")";
        event.metadata["nid"] = nid;
        event.metadata["function_name"] = name;
        event.metadata["module"] = module;
        event.numeric_data["caller_addr"] = static_cast<int64_t>(caller_addr);
        
        emit_event(event);
    }
    
    /**
     * @brief Update import call status (assumes lock held)
     */
    void update_import_call_internal(const std::string& nid, bool success, Timestamp timestamp) {
        auto it = nid_index_.find(nid);
        if (it == nid_index_.end()) {
            return;
        }
        
        auto& entry = imports_[it->second];
        entry.call_count++;
        entry.last_call = timestamp;
        
        // Update status based on call outcome
        if (success) {
            if (entry.status == ImportStatus::STUB_IMPLEMENTED ||
                entry.status == ImportStatus::MISSING_NOT_CALLED) {
                entry.status = ImportStatus::CALLED_SUCCESSFULLY;
            }
        } else {
            if (entry.status == ImportStatus::STUB_IMPLEMENTED ||
                entry.status == ImportStatus::CALLED_SUCCESSFULLY) {
                entry.status = ImportStatus::IMPLEMENTED_BUT_FAILED;
            } else if (entry.status == ImportStatus::MISSING_NOT_CALLED) {
                entry.status = ImportStatus::MISSING_CALLED;  // HIGH RISK
            }
        }
    }
    
    /**
     * @brief Calculate current statistics (assumes lock held)
     */
    ImportStatistics calculate_statistics() const {
        ImportStatistics stats;
        stats.total_imports = imports_.size();
        
        for (const auto& imp : imports_) {
            switch (imp.status) {
                case ImportStatus::MISSING_NOT_CALLED:
                    stats.missing_not_called++;
                    break;
                case ImportStatus::MISSING_CALLED:
                    stats.missing_called++;  // HIGH RISK
                    break;
                case ImportStatus::IMPLEMENTED_BUT_FAILED:
                    stats.implemented_but_failed++;
                    stats.resolved_imports++;
                    break;
                case ImportStatus::CALLED_SUCCESSFULLY:
                    stats.called_successfully++;
                    stats.resolved_imports++;
                    break;
                case ImportStatus::STUB_IMPLEMENTED:
                    stats.stub_implemented++;
                    stats.resolved_imports++;
                    break;
            }
        }
        
        return stats;
    }
    
    /**
     * @brief Generate comprehensive report (assumes lock held)
     */
    std::string generate_report_internal() const {
        std::ostringstream json;
        ImportStatistics stats = calculate_statistics();
        
        json << "{\n";
        json << "  \"plugin\": \"" << name() << "\",\n";
        json << "  \"version\": \"" << version() << "\",\n";
        json << "  \"generated_at_ms\": " << std::fixed << std::setprecision(3)
             << timestamp_to_ms(now()) << ",\n";
        
        // Statistics
        json << "  \"statistics\": " << stats.to_json() << ",\n";
        
        // High-risk imports summary
        json << "  \"high_risk_count\": " << stats.missing_called << ",\n";
        
        // Imports array (limited to avoid huge output)
        json << "  \"imports\": [\n";
        size_t export_count = std::min(imports_.size(), static_cast<size_t>(1000));
        for (size_t i = 0; i < export_count; ++i) {
            json << "    " << imports_[i].to_json();
            if (i < export_count - 1) json << ",";
            json << "\n";
        }
        if (imports_.size() > export_count) {
            json << "    ,\"... (" << (imports_.size() - export_count) << " more imports truncated)\"\n";
        }
        json << "  ],\n";
        
        // Missing imports list
        json << "  \"missing_imports\": [\n";
        std::vector<size_t> missing_indices;
        for (size_t i = 0; i < imports_.size(); ++i) {
            if (imports_[i].status == ImportStatus::MISSING_NOT_CALLED ||
                imports_[i].status == ImportStatus::MISSING_CALLED) {
                missing_indices.push_back(i);
            }
        }
        for (size_t i = 0; i < missing_indices.size(); ++i) {
            json << "    " << imports_[missing_indices[i]].to_json();
            if (i < missing_indices.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ]\n";
        
        json << "}\n";
        
        return json.str();
    }
    
    /**
     * @brief Emit an info-level diagnostic event
     */
    void emit_info(const std::string& message) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "info";
        event.severity = Severity::INFO;
        event.message = message;
        emit_event(event);
    }
    
    /**
     * @brief Emit a warning diagnostic event
     */
    void emit_warning(const std::string& message) {
        DiagnosticEvent event;
        event.source_plugin = name();
        event.event_type = "warning";
        event.severity = Severity::WARNING;
        event.message = message;
        emit_event(event);
    }
    
    //=========================================================================
    // Member Variables
    //=========================================================================
    
    std::vector<ImportEntry> imports_;                          ///< All tracked imports
    std::unordered_map<std::string, size_t> nid_index_;         ///< NID -> index in imports_
    std::unordered_multimap<std::string, size_t> name_index_;   ///< Name -> index (multiple NIDs can have same name)
    std::unordered_multimap<std::string, size_t> module_index_; ///< Module -> index
    
    size_t max_events_;
    bool track_callers_;
};

} // namespace diagnostics
} // namespace prosper
