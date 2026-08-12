#pragma once
#include "../core/diagnostic_interface.hpp"
#include "../core/event_bus.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <algorithm>

namespace prosper {
namespace diagnostics {

//=============================================================================
// Module Load Plugin - Phase 9.5 Diagnostic Plugin
//
// Tracks module loading/unloading events for PS4 emulator diagnostics.
// Records module load/unload events with timestamps, tracks dependencies,
// records base addresses and sizes, detects circular/missing dependencies.
//=============================================================================

/**
 * @brief Represents information about a loaded module
 */
struct ModuleInfo {
    std::string name;                       ///< Module name (e.g., "libSceLibcInternal")
    std::string path;                       ///< File path to the module
    uint64_t base_addr{0};                  ///< Base load address in memory
    size_t size{0};                         ///< Size of the module in bytes
    Timestamp loaded;                       ///< When the module was loaded
    Timestamp unloaded{Timestamp::min()};   ///< When the module was unloaded (min if still loaded)
    std::vector<std::string> dependencies;  ///< List of modules this one depends on
    bool is_prx{false};                     ///< True if this is a PRX (relocatable) module
    bool is_main_executable{false};         ///< True if this is the main ELF executable
    
    /// Default constructor
    ModuleInfo() = default;
    
    /// Constructor with essential fields
    ModuleInfo(const std::string& n, const std::string& p, uint64_t addr, size_t sz)
        : name(n), path(p), base_addr(addr), size(sz), loaded(now()) {}
    
    /// Check if module is currently loaded
    bool is_loaded() const {
        return unloaded.time_since_epoch().count() != 0;
    }
    
    /// Get end address of module
    uint64_t end_address() const {
        return base_addr + size;
    }
    
    /// Serialize to JSON string
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"name\":\"" << name << "\",";
        oss << "\"path\":\"" << path << "\",";
        oss << "\"base_addr\":\"0x" << std::hex << base_addr << std::dec << "\",";
        oss << "\"size\":" << size << ",";
        oss << "\"loaded_ms\":" << std::fixed << std::setprecision(3) << timestamp_to_ms(loaded) << ",";
        
        if (unloaded.time_since_epoch().count() != 0) {
            oss << "\"unloaded_ms\":" << std::fixed << std::setprecision(3) 
                << timestamp_to_ms(unloaded) << ",";
            oss << "\"currently_loaded\":false,";
        } else {
            oss << "\"currently_loaded\":true,";
        }
        
        oss << "\"is_prx\":" << (is_prx ? "true" : "false") << ",";
        oss << "\"is_main_executable\":" << (is_main_executable ? "true" : "false") << ",";
        
        // Dependencies array
        oss << "\"dependencies\":[";
        for (size_t i = 0; i < dependencies.size(); ++i) {
            oss << "\"" << dependencies[i] << "\"";
            if (i < dependencies.size() - 1) oss << ",";
        }
        oss << "]";
        
        oss << "}";
        return oss.str();
    }
};

/**
 * @brief Dependency analysis result structure
 */
struct DependencyAnalysisResult {
    bool has_circular_dependencies{false};
    bool has_missing_dependencies{false};
    std::vector<std::vector<std::string>> circular_chains;
    std::vector<std::string> missing_modules;
    std::vector<std::string> unresolved_symbols;
    
    std::string to_json() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"has_circular_dependencies\":" << (has_circular_dependencies ? "true" : "false") << ",";
        oss << "\"has_missing_dependencies\":" << (has_missing_dependencies ? "true" : "false") << ",";
        
        // Circular dependency chains
        oss << "\"circular_chains\":[";
        for (size_t i = 0; i < circular_chains.size(); ++i) {
            oss << "[";
            for (size_t j = 0; j < circular_chains[i].size(); ++j) {
                oss << "\"" << circular_chains[i][j] << "\"";
                if (j < circular_chains[i].size() - 1) oss << ",";
            }
            oss << "]";
            if (i < circular_chains.size() - 1) oss << ",";
        }
        oss << "],";
        
        // Missing modules
        oss << "\"missing_modules\":[";
        for (size_t i = 0; i < missing_modules.size(); ++i) {
            oss << "\"" << missing_modules[i] << "\"";
            if (i < missing_modules.size() - 1) oss << ",";
        }
        oss << "]";
        
        oss << "}";
        return oss.str();
    }
};

/**
 * @brief Module Load Plugin implementation
 * 
 * This plugin provides comprehensive module tracking:
 * - Records all module load/unload events with full metadata
 * - Tracks inter-module dependencies
 * - Records memory layout information (base addresses, sizes)
 * - Detects circular dependencies and missing required modules
 * - Generates dependency graphs and reports
 */
class ModuleLoadPlugin : public DiagnosticPlugin {
public:
    //=========================================================================
    // Constructor / Destructor
    //=========================================================================
    
    ModuleLoadPlugin()
        : total_modules_loaded_(0),
          total_modules_unloaded_(0),
          max_events_(10000) {}
    
    virtual ~ModuleLoadPlugin() override {
        shutdown();
    }
    
    //=========================================================================
    // DiagnosticPlugin Interface Implementation
    //=========================================================================
    
    std::string name() const override { return "ModuleLoad"; }
    
    std::string version() const override { return "1.5.0"; }
    
    std::string description() const override {
        return "Tracks module loading, dependencies, and memory layout";
    }
    
    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        modules_.clear();
        module_name_index_.clear();
        module_addr_index_.clear();
        total_modules_loaded_ = 0;
        total_modules_unloaded_ = 0;
        
        active_ = true;
        
        // Parse configuration
        auto it = config_.find("max_events");
        if (it != config_.end()) {
            try { max_events_ = static_cast<size_t>(std::stoull(it->second)); } catch (...) {}
        }
        
        it = config_.find("track_dependencies");
        if (it != config_.end()) {
            track_dependencies_ = (it->second == "true" || it->second == "1");
        }
        
        emit_info("ModuleLoad initialized successfully");
        return true;
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
        emit_info("ModuleLoad shut down");
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        modules_.clear();
        module_name_index_.clear();
        module_addr_index_.clear();
        total_modules_loaded_ = 0;
        total_modules_unloaded_ = 0;
        event_count_ = 0;
    }
    
    void on_event(const DiagnosticEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!active_) return;
        if (event_count_ >= max_events_) return;
        
        // Process module-related events
        if (event.event_type == "module_loaded") {
            auto name_it = event.metadata.find("module_name");
            auto path_it = event.metadata.find("module_path");
            
            if (name_it != event.metadata.end()) {
                uint64_t base_addr = 0;
                size_t size = 0;
                
                auto addr_it = event.numeric_data.find("base_addr");
                if (addr_it != event.numeric_data.end()) {
                    base_addr = static_cast<uint64_t>(addr_it->second);
                }
                
                auto size_it = event.numeric_data.find("module_size");
                if (size_it != event.numeric_data.end()) {
                    size = static_cast<size_t>(size_it->second);
                }
                
                on_module_loaded_internal(name_it->second,
                    path_it != event.metadata.end() ? path_it->second : "",
                    base_addr, size, event.timestamp);
            }
        }
        else if (event.event_type == "module_unloaded") {
            auto name_it = event.metadata.find("module_name");
            if (name_it != event.metadata.end()) {
                on_module_unloaded_internal(name_it->second, event.timestamp);
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
     * @brief Record that a module was loaded
     * @param name Module name
     * @param path File path to the module
     * @param base_addr Base address where module is loaded
     * @param size Size of the module in bytes
     * @param timestamp Optional timestamp (defaults to now)
     */
    void on_module_loaded(const std::string& name, 
                          const std::string& path,
                          uint64_t base_addr,
                          size_t size,
                          Timestamp timestamp = Timestamp{}) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_module_loaded_internal(name, path, base_addr, size, 
                                  timestamp.time_since_epoch().count() == 0 ? now() : timestamp);
    }
    
    /**
     * @brief Record that a module was unloaded
     * @param name Name of the unloaded module
     * @param timestamp Optional timestamp (defaults to now)
     */
    void on_module_unloaded(const std::string& name, Timestamp timestamp = Timestamp{}) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_module_unloaded_internal(name, 
                                    timestamp.time_since_epoch().count() == 0 ? now() : timestamp);
    }
    
    /**
     * @brief Get information about a specific module by name
     * @param name Module name to look up
     * @return Pointer to ModuleInfo or nullptr if not found
     */
    const ModuleInfo* get_module_info(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = module_name_index_.find(name);
        if (it != module_name_index_.end()) {
            return &modules_[it->second];
        }
        return nullptr;
    }
    
    /**
     * @brief Get information about a module by its base address
     * @param addr Base address to look up
     * @return Pointer to ModuleInfo or nullptr if not found
     */
    const ModuleInfo* get_module_by_address(uint64_t addr) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Find module whose range contains this address
        for (const auto& mod : modules_) {
            if (mod.unloaded.time_since_epoch().count() == 0 &&  // Still loaded
                addr >= mod.base_addr && addr < mod.base_addr + mod.size) {
                return &mod;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Get the complete dependency graph as JSON
     * @return JSON string representing the dependency graph
     */
    std::string get_dependency_graph() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return generate_dependency_graph_internal();
    }
    
    /**
     * @brief Analyze dependencies for issues
     * @return DependencyAnalysisResult with any detected problems
     */
    DependencyAnalysisResult analyze_dependencies() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return analyze_dependencies_internal();
    }
    
    /**
     * @brief Add a dependency relationship between two modules
     * @param module_name The dependent module
     * @param depends_on The module being depended upon
     */
    void add_dependency(const std::string& module_name, const std::string& depends_on) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = module_name_index_.find(module_name);
        if (it != module_name_index_.end()) {
            modules_[it->second].dependencies.push_back(depends_on);
        }
    }
    
    /**
     * @brief Set whether a module is a PRX
     * @param name Module name
     * @param is_prx True if PRX module
     */
    void set_is_prx(const std::string& name, bool is_prx) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = module_name_index_.find(name);
        if (it != module_name_index_.end()) {
            modules_[it->second].is_prx = is_prx;
        }
    }
    
    /**
     * @brief Mark a module as the main executable
     * @param name Module name
     */
    void set_as_main_executable(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = module_name_index_.find(name);
        if (it != module_name_index_.end()) {
            modules_[it->second].is_main_executable = true;
        }
    }
    
    /**
     * @brief Get count of currently loaded modules
     */
    size_t loaded_module_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t count = 0;
        for (const auto& mod : modules_) {
            if (mod.unloaded.time_since_epoch().count() == 0) {
                count++;
            }
        }
        return count;
    }
    
    /**
     * @brief Get total number of modules ever loaded
     */
    size_t total_loaded() const { return total_modules_loaded_; }
    
    /**
     * @brief Get list of all currently loaded module names
     */
    std::vector<std::string> get_loaded_module_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> names;
        for (const auto& mod : modules_) {
            if (mod.unloaded.time_since_epoch().count() == 0) {
                names.push_back(mod.name);
            }
        }
        return names;
    }
    
    /**
     * @brief Get all modules (including unloaded ones)
     */
    std::vector<ModuleInfo> get_all_modules() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return modules_;
    }

private:
    //=========================================================================
    // Internal Implementation Methods
    //=========================================================================
    
    /**
     * @brief Internal module load handler (assumes lock held)
     */
    void on_module_loaded_internal(const std::string& name,
                                   const std::string& path,
                                   uint64_t base_addr,
                                   size_t size,
                                   Timestamp timestamp) {
        // Check for duplicate
        auto existing = module_name_index_.find(name);
        if (existing != module_name_index_.end()) {
            // Update existing entry
            modules_[existing->second].base_addr = base_addr;
            modules_[existing->second].size = size;
            modules_[existing->second].path = path;
            modules_[existing->second].loaded = timestamp;
            modules_[existing->second].unloaded = Timestamp::min();  // Reset unload time
            
            emit_warning("Module reloaded: " + name);
            return;
        }
        
        // Create new module info
        ModuleInfo info(name, path, base_addr, size);
        info.loaded = timestamp;
        
        size_t index = modules_.size();
        modules_.push_back(std::move(info));
        module_name_index_[name] = index;
        module_addr_index_[base_addr] = index;
        
        total_modules_loaded_++;
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = DiagnosticPlugin::name();
        event.event_type = "module_tracked";
        event.severity = Severity::INFO;
        event.message = "Module loaded: " + name;
        event.metadata["module_name"] = name;
        event.metadata["module_path"] = path;
        event.numeric_data["base_addr"] = static_cast<int64_t>(base_addr);
        event.numeric_data["size"] = static_cast<int64_t>(size);
        event.numeric_data["total_modules"] = static_cast<int64_t>(total_modules_loaded_);
        
        emit_event(event);
    }
    
    /**
     * @brief Internal module unload handler (assumes lock held)
     */
    void on_module_unloaded_internal(const std::string& name, Timestamp timestamp) {
        auto it = module_name_index_.find(name);
        if (it == module_name_index_.end()) {
            emit_warning("Attempted to unload unknown module: " + name);
            return;
        }
        
        modules_[it->second].unloaded = timestamp;
        total_modules_unloaded_++;
        
        // Emit diagnostic event
        DiagnosticEvent event;
        event.source_plugin = DiagnosticPlugin::name();
        event.event_type = "module_unload_tracked";
        event.severity = Severity::INFO;
        event.message = "Module unloaded: " + name;
        event.metadata["module_name"] = name;
        event.numeric_data["total_unloaded"] = static_cast<int64_t>(total_modules_unloaded_);
        
        emit_event(event);
    }
    
    /**
     * @brief Generate dependency graph JSON (assumes lock held)
     */
    std::string generate_dependency_graph_internal() const {
        std::ostringstream json;
        
        json << "{\n";
        json << "  \"nodes\": [\n";
        
        size_t node_idx = 0;
        for (const auto& mod : modules_) {
            json << "    {\"id\":\"" << mod.name << "\","
                 << "\"label\":\"" << mod.name << "\","
                 << "\"type\":" << (mod.is_prx ? "\"prx\"" : (mod.is_main_executable ? "\"main\"" : "\"lib\"")) << ","
                 << "\"base_addr\":\"0x" << std::hex << mod.base_addr << std::dec << "\","
                 << "\"loaded\":" << (mod.unloaded.time_since_epoch().count() == 0 ? "true" : "false")
                 << "}";
            if (node_idx++ < modules_.size() - 1) json << ",";
            json << "\n";
        }
        
        json << "  ],\n";
        json << "  \"edges\": [\n";
        
        size_t edge_idx = 0;
        for (const auto& mod : modules_) {
            for (const auto& dep : mod.dependencies) {
                json << "    {\"source\":\"" << mod.name << "\","
                     << "\"target\":\"" << dep << "\"}";
                if (edge_idx++ > 0 || edge_idx < count_all_dependencies() - 1) json << ",";
                json << "\n";
            }
        }
        
        json << "  ]\n";
        json << "}\n";
        
        return json.str();
    }
    
    /**
     * @brief Count total dependencies across all modules
     */
    size_t count_all_dependencies() const {
        size_t count = 0;
        for (const auto& mod : modules_) {
            count += mod.dependencies.size();
        }
        return count;
    }
    
    /**
     * @brief Analyze dependencies for circular references and missing modules
     */
    DependencyAnalysisResult analyze_dependencies_internal() const {
        DependencyAnalysisResult result;
        
        // Build set of known module names
        std::unordered_set<std::string> known_modules;
        for (const auto& mod : modules_) {
            if (mod.unloaded.time_since_epoch().count() == 0) {  // Only check loaded modules
                known_modules.insert(mod.name);
            }
        }
        
        // Check for missing dependencies
        for (const auto& mod : modules_) {
            if (mod.unloaded.time_since_epoch().count() != 0) continue;
            
            for (const auto& dep : mod.dependencies) {
                if (known_modules.find(dep) == known_modules.end()) {
                    result.has_missing_dependencies = true;
                    result.missing_modules.push_back(dep + " (required by " + mod.name + ")");
                }
            }
        }
        
        // Detect circular dependencies using DFS
        std::unordered_set<std::string> visiting;
        std::unordered_set<std::string> visited;
        std::vector<std::string> current_path;
        
        for (const auto& mod : modules_) {
            if (mod.unloaded.time_since_epoch().count() != 0) continue;
            if (visited.find(mod.name) != visited.end()) continue;
            
            detect_cycles(mod.name, visiting, visited, current_path, result);
        }
        
        return result;
    }
    
    /**
     * @brief DFS-based cycle detection helper
     */
    void detect_cycles(const std::string& node,
                      std::unordered_set<std::string>& visiting,
                      std::unordered_set<std::string>& visited,
                      std::vector<std::string>& path,
                      DependencyAnalysisResult& result) const {
        
        visiting.insert(node);
        path.push_back(node);
        
        auto it = module_name_index_.find(node);
        if (it != module_name_index_.end()) {
            for (const auto& dep : modules_[it->second].dependencies) {
                if (visiting.find(dep) != visiting.end()) {
                    // Found cycle - extract the cycle
                    result.has_circular_dependencies = true;
                    std::vector<std::string> cycle;
                    
                    auto cycle_start = std::find(path.begin(), path.end(), dep);
                    if (cycle_start != path.end()) {
                        cycle.assign(cycle_start, path.end());
                        cycle.push_back(dep);  // Complete the loop
                        result.circular_chains.push_back(cycle);
                    }
                }
                else if (visited.find(dep) == visited.end()) {
                    detect_cycles(dep, visiting, visited, path, result);
                }
            }
        }
        
        path.pop_back();
        visiting.erase(node);
        visited.insert(node);
    }
    
    /**
     * @brief Generate comprehensive report (assumes lock held)
     */
    std::string generate_report_internal() const {
        std::ostringstream json;
        
        json << "{\n";
        json << "  \"plugin\": \"" << name() << "\",\n";
        json << "  \"version\": \"" << version() << "\",\n";
        json << "  \"generated_at_ms\": " << std::fixed << std::setprecision(3)
             << timestamp_to_ms(now()) << ",\n";
        json << "  \"statistics\": {\n";
        json << "    \"total_ever_loaded\": " << total_modules_loaded_ << ",\n";
        json << "    \"total_unloaded\": " << total_modules_unloaded_ << ",\n";
        json << "    \"currently_loaded\": " << loaded_module_count_locked() << "\n";
        json << "  },\n";
        
        // Modules array
        json << "  \"modules\": [\n";
        for (size_t i = 0; i < modules_.size(); ++i) {
            json << "    " << modules_[i].to_json();
            if (i < modules_.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
        
        // Dependency analysis
        json << "  \"dependency_analysis\": " << analyze_dependencies_internal().to_json() << "\n";
        
        json << "}\n";
        
        return json.str();
    }
    
    /**
     * @brief Loaded module count (assumes lock held)
     */
    size_t loaded_module_count_locked() const {
        size_t count = 0;
        for (const auto& mod : modules_) {
            if (mod.unloaded.time_since_epoch().count() == 0) {
                count++;
            }
        }
        return count;
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
    
    std::vector<ModuleInfo> modules_;              ///< All tracked modules (including unloaded)
    std::unordered_map<std::string, size_t> module_name_index_;  ///< Name -> index in modules_
    std::unordered_map<uint64_t, size_t> module_addr_index_;     ///< Base addr -> index in modules_
    
    size_t total_modules_loaded_;
    size_t total_modules_unloaded_;
    size_t max_events_;
    bool track_dependencies_{true};
};

} // namespace diagnostics
} // namespace prosper
