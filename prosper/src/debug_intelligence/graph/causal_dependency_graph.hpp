/**
 * Causal Dependency Graph - Priority 6
 * 
 * PROBLEM SOLVED:
 * Emulator debugging is not normal application debugging.
 * Architecture has multiple layers:
 * 
 *   Guest Application
 *        ↓
 *   HLE (High-Level Emulation)
 *        ↓
 *   Kernel
 *        ↓
 *   Memory
 *        ↓
 *   CPU
 *        ↓
 *   GPU
 *        ↓
 *   Host System
 * 
 * A crash in one layer may originate elsewhere.
 * 
 * EXAMPLE:
 *   Crash: Allocator failure (Memory layer)
 *   Actual cause: GPU wrote into freed memory 20 seconds earlier (GPU layer)
 * 
 * This module builds and queries dependency graphs to trace root causes across layers.
 */

#pragma once

#include "../core/foundation.hpp"
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <queue>
#include <functional>
#include <atomic>
#include <shared_mutex>
#include <thread>

namespace prosper_debug {
namespace graph {

// ============================================================================
// Layer Definitions
// ============================================================================

enum class EmulationLayer : uint8_t {
    Guest,          // Game/application code
    HLE,            // High-level emulation functions
    Kernel,         // OS kernel simulation
    Memory,         // Memory management
    CPU,            // Processor emulation
    GPU,            // Graphics processing
    Audio,          // Sound processing
    Input,          // Input handling
    FileSystem,     // File system abstraction
    Network,        // Network stack
    Host,           // Host operating system
    Diagnostic      // Debug/diagnostic infrastructure
};

inline std::string emulationLayerToString(EmulationLayer layer) {
    switch (layer) {
        case EmulationLayer::Guest: return "Guest";
        case EmulationLayer::HLE: return "HLE";
        case EmulationLayer::Kernel: return "Kernel";
        case EmulationLayer::Memory: return "Memory";
        case EmulationLayer::CPU: return "CPU";
        case EmulationLayer::GPU: return "GPU";
        case EmulationLayer::Audio: return "Audio";
        case EmulationLayer::Input: return "Input";
        case EmulationLayer::FileSystem: return "FileSystem";
        case EmulationLayer::Network: return "Network";
        case EmulationLayer::Host: return "Host";
        case EmulationLayer::Diagnostic: return "Diagnostic";
    }
    return "Unknown";
}

inline EmulationLayer subsystemToLayer(Subsystem subs) {
    switch (subs) {
        case Subsystem::GuestApplication:
        case Subsystem::IL2CPP_Runtime:
        case Subsystem::GameCode:
            return EmulationLayer::Guest;
            
        case Subsystem::HLE_LibKernel:
        case Subsystem::HLE_LibGpu:
        case Subsystem::HLE_LibAudio:
        case Subsystem::HLE_LibNet:
        case Subsystem::HLE_LibPng:
        case Subsystem::HLE_LibJpeg:
        case Subsystem::HLE_LibZlib:
        case Subsystem::HLE_LibSsl:
        case Subsystem::HLE_LibCrypto:
        case Subsystem::HLE_Other:
            return EmulationLayer::HLE;
            
        case Subsystem::Kernel:
        case Subsystem::ThreadManager:
        case Subsystem::Synchronization:
            return EmulationLayer::Kernel;
            
        case Subsystem::Memory:
            return EmulationLayer::Memory;
            
        case Subsystem::CPU:
            return EmulationLayer::CPU;
            
        case Subsystem::GPU:
            return EmulationLayer::GPU;
            
        case Subsystem::Audio:
            return EmulationLayer::Audio;
            
        case Subsystem::Input:
            return EmulationLayer::Input;
            
        case Subsystem::FileSystem:
        case Subsystem::FileSystemHost:
            return EmulationLayer::FileSystem;
            
        case Subsystem::HostSystem:
            return EmulationLayer::Host;
            
        default:
            return EmulationLayer::Diagnostic;
    }
}

// ============================================================================
// Node Types for the Graph
// ============================================================================

enum class NodeType : uint8_t {
    // Events
    Crash,
    Error,
    Corruption,
    Warning,
    StateChange,
    ResourceEvent,
    
    // Entities
    MemoryRegion,
    Object,
    Handle,
    Thread,
    Allocation,
    
    // Code locations
    FunctionCall,
    CodePath,
    BranchPoint,
    
    // Abstract concepts
    RootCauseCandidate,
    Symptom,
    Effect
};

inline std::string nodeTypeToString(NodeType type) {
    switch (type) {
        case NodeType::Crash: return "Crash";
        case NodeType::Error: return "Error";
        case NodeType::Corruption: return "Corruption";
        case NodeType::Warning: return "Warning";
        case NodeType::StateChange: return "StateChange";
        case NodeType::ResourceEvent: return "ResourceEvent";
        case NodeType::MemoryRegion: return "MemoryRegion";
        case NodeType::Object: return "Object";
        case NodeType::Handle: return "Handle";
        case NodeType::Thread: return "Thread";
        case NodeType::Allocation: return "Allocation";
        case NodeType::FunctionCall: return "FunctionCall";
        case NodeType::CodePath: return "CodePath";
        case NodeType::BranchPoint: return "BranchPoint";
        case NodeType::RootCauseCandidate: return "RootCauseCandidate";
        case NodeType::Symptom: return "Symptom";
        case NodeType::Effect: return "Effect";
    }
    return "Unknown";
}

// ============================================================================
// Edge Types (Causal Relationships)
// ============================================================================

enum class EdgeType : uint8_t {
    // Direct causation
    CausedBy,           // A was directly caused by B
    CorruptedBy,        // A's data was corrupted by B
    TriggeredBy,        // A was triggered by B
    
    // Temporal relationships
    HappensBefore,      // A occurs before B in time
    HappensAfter,       // A occurs after B in time
    ConcurrentWith,     // A and B happen at same time
    
    // Containment/ownership
    Contains,           // A contains B (spatial or logical)
    Owns,              // A owns resource B
    Allocates,          // A allocated memory that became B
    Frees,             // A freed memory that B used
    
    // Dependencies
    DependsOn,          // A depends on B
    ReadsFrom,          // A reads data written by B
    WritesTo,          // A writes data read by B
    Calls,              // A calls function B
    CalledBy,           // A is called by B
    
    // Cross-layer
    CrossLayerUp,       // From lower layer to higher layer
    CrossLayerDown,     // From higher layer to lower layer
    
    // Abstract
    RelatedTo,          // General relationship
    SimilarTo,          // Pattern similarity
    PossiblyCauses      // Speculative causal link
};

inline std::string edgeTypeToString(EdgeType type) {
    switch (type) {
        case EdgeType::CausedBy: return "caused_by";
        case EdgeType::CorruptedBy: return "corrupted_by";
        case EdgeType::TriggeredBy: return "triggered_by";
        case EdgeType::HappensBefore: return "happens_before";
        case EdgeType::HappensAfter: return "happens_after";
        case EdgeType::ConcurrentWith: return "concurrent_with";
        case EdgeType::Contains: return "contains";
        case EdgeType::Owns: return "owns";
        case EdgeType::Allocates: return "allocates";
        case EdgeType::Frees: return "frees";
        case EdgeType::DependsOn: return "depends_on";
        case EdgeType::ReadsFrom: return "reads_from";
        case EdgeType::WritesTo: return "writes_to";
        case EdgeType::Calls: return "calls";
        case EdgeType::CalledBy: return "called_by";
        case EdgeType::CrossLayerUp: return "cross_layer_up";
        case EdgeType::CrossLayerDown: return "cross_layer_down";
        case EdgeType::RelatedTo: return "related_to";
        case EdgeType::SimilarTo: return "similar_to";
        case EdgeType::PossiblyCauses: return "possibly_causes";
    }
    return "unknown";
}

// ============================================================================
// Graph Node
// ============================================================================

struct GraphNode {
    std::string id;
    NodeType type;
    EmulationLayer layer{EmulationLayer::Diagnostic};
    
    // Identification
    std::string name;
    std::string description;
    
    // Location/context
    SourceLocation location;
    Subsystem subsystem{Subsystem::Unknown};
    
    // Timing
    DebugTimestamp timestamp;
    uint64_t frame_number{0};
    
    // Data payload
    std::map<std::string, std::string> attributes;
    
    // State
    bool is_resolved{false};
    bool is_symptom{false};      // This is a symptom, not a cause
    bool is_root_cause{false};   // This is identified as root cause
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "node_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    GraphNode() {
        id = generateId();
        timestamp = DebugTimestamp::now();
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << JsonUtils::escapeJson(id) << "\",\n";
        json << "  \"type\": \"" << nodeTypeToString(type) << "\",\n";
        json << "  \"layer\": \"" << emulationLayerToString(layer) << "\",\n";
        json << "  \"name\": \"" << JsonUtils::escapeJson(name) << "\",\n";
        if (!description.empty()) {
            json << "  \"description\": \"" << JsonUtils::escapeJson(description) << "\",\n";
        }
        json << "  \"frame\": " << frame_number << "\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Graph Edge
// ============================================================================

struct GraphEdge {
    std::string id;
    std::string source_id;   // From node
    std::string target_id;   // To node
    EdgeType type;
    
    // Strength/confidence
    double confidence{1.0};  // How confident are we in this relationship?
    
    // Evidence
    std::vector<std::string> evidence_ids;
    std::string reasoning;
    
    // Metadata
    std::map<std::string, std::string> attributes;
    
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "edge_" + std::to_string(now) + "_" + std::to_string(++counter);
    }
    
    GraphEdge() {
        id = generateId();
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"id\": \"" << JsonUtils::escapeJson(id) << "\",\n";
        json << "  \"source\": \"" << JsonUtils::escapeJson(source_id) << "\",\n";
        json << "  \"target\": \"" << JsonUtils::escapeJson(target_id) << "\",\n";
        json << "  \"type\": \"" << edgeTypeToString(type) << "\",\n";
        json << "  \"confidence\": " << std::fixed << std::setprecision(2) 
             << confidence << "\n";
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Causal Path - Sequence of edges from cause to effect
// ============================================================================

struct CausalPath {
    std::vector<std::string> node_ids;  // Nodes along the path
    std::vector<GraphEdge> edges;        // Edges connecting them
    
    double total_confidence{1.0};
    int cross_layer_transitions{0};
    
    // Classification
    enum class Strength {
        Strong,     // High confidence, direct evidence
        Moderate,   // Reasonable inference
        Weak,       // Speculative but plausible
        VeryWeak    // Tentative, needs verification
    } strength{Strength::Moderate};
    
    double strengthScore() const {
        switch (strength) {
            case Strength::Strong: return 0.9;
            case Strength::Moderate: return 0.7;
            case Strength::Weak: return 0.4;
            case Strength::VeryWeak: return 0.2;
        }
        return 0.0;
    }
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"nodes\": " << node_ids.size() << ",\n";
        json << "  \"edges\": " << edges.size() << ",\n";
        json << "  \"confidence\": " << std::fixed << std::setprecision(2) 
             << total_confidence << ",\n";
        
        const char* str_str = "";
        switch (strength) {
            case Strength::Strong: str_str = "Strong"; break;
            case Strength::Moderate: str_str = "Moderate"; break;
            case Strength::Weak: str_str = "Weak"; break;
            case Strength::VeryWeak: str_str = "VeryWeak"; break;
        }
        json << "  \"strength\": \"" << str_str << "\"\n";
        
        json << "}";
        return json.str();
    }
};

// ============================================================================
// Root Cause Analysis Result
// ============================================================================

struct RootCauseAnalysis {
    GraphNode symptom_node;              // The crash/error we're investigating
    std::vector<CausalPath> candidate_paths;  // Possible root causes
    
    // Ranked candidates
    struct Candidate {
        GraphNode node;
        CausalPath path;
        double overall_score{0};
        std::string reasoning;
        
        bool operator>(const Candidate& other) const {
            return overall_score > other.overall_score;
        }
    };
    
    std::vector<Candidate> ranked_candidates;
    
    // Summary
    GraphNode most_likely_root_cause;
    CausalPath most_likely_path;
    double confidence_in_analysis{0};
    
    // Rejected hypotheses
    std::vector<Candidate> rejected_candidates;
    std::vector<std::string> rejection_reasons;
    
    std::string toJson() const {
        std::stringstream json;
        json << "{\n";
        json << "  \"symptom\": \"" << JsonUtils::escapeJson(symptom_node.name) << "\",\n";
        json << "  \"candidates\": " << ranked_candidates.size() << ",\n";
        json << "  \"most_likely_cause\": ";
        if (!most_likely_root_cause.id.empty()) {
            json << "\"" << JsonUtils::escapeJson(most_likely_root_cause.name) << "\"\n";
        } else {
            json << "null\n";
        }
        json << "}\n";
        return json.str();
    }
};

// ============================================================================
// Causal Dependency Graph - Main Class
// ============================================================================

class CausalDependencyGraph {
public:
    struct Config {
        size_t max_nodes;
        size_t max_edges;
        bool auto_detect_cross_layer;
        bool infer_temporal_relations;
        int max_path_length;  // Max nodes in a causal path
        
        Config()
            : max_nodes(100000)
            , max_edges(500000)
            , auto_detect_cross_layer(true)
            , infer_temporal_relations(true)
            , max_path_length(10) {}
        
        Config(size_t mn, size_t me, bool adcl, bool itr, int mpl)
            : max_nodes(mn)
            , max_edges(me)
            , auto_detect_cross_layer(adcl)
            , infer_temporal_relations(itr)
            , max_path_length(mpl) {}
        
        static Config defaultConfig() {
            return Config(100000, 500000, true, true, 10);
        }
    };
    
    explicit CausalDependencyGraph(const Config& config = Config())
        : m_config(config), m_enabled(true) {}
    
    void enable() { m_enabled.store(true); }
    void disable() { m_enabled.store(false); }
    bool isEnabled() const { return m_enabled.load(); }
    
    void clear() {
        std::unique_lock lock(m_mutex);
        m_nodes.clear();
        m_edges.clear();
        m_adjacency.clear();
    }
    
    // ========================================================================
    // Node Management
    // ========================================================================
    
    /**
     * Add a node to the graph
     */
    std::string addNode(
        NodeType type,
        const std::string& name,
        EmulationLayer layer,
        const SourceLocation& loc = DEBUG_HERE(),
        Subsystem subs = Subsystem::Unknown) {
        
        if (!isEnabled()) return "";
        
        GraphNode node;
        node.type = type;
        node.name = name;
        node.layer = layer;
        node.location = loc;
        node.subsystem = subs;
        
        std::unique_lock lock(m_mutex);
        
        if (m_nodes.size() >= m_config.max_nodes) return "";
        
        m_nodes[node.id] = node;
        m_adjacency[node.id];  // Initialize adjacency list
        
        return node.id;
    }
    
    /**
     * Get node by ID
     */
    std::optional<GraphNode> getNode(const std::string& id) const {
        std::shared_lock lock(m_mutex);
        
        auto it = m_nodes.find(id);
        if (it != m_nodes.end()) return it->second;
        return std::nullopt;
    }
    
    /**
     * Find nodes matching criteria
     */
    std::vector<GraphNode> findNodes(
        std::optional<NodeType> type_filter = std::nullopt,
        std::optional<EmulationLayer> layer_filter = std::nullopt,
        std::optional<Subsystem> subsystem_filter = std::nullopt) const {
        
        std::shared_lock lock(m_mutex);
        
        std::vector<GraphNode> result;
        for (const auto& [id, node] : m_nodes) {
            if (type_filter.has_value() && node.type != *type_filter) continue;
            if (layer_filter.has_value() && node.layer != *layer_filter) continue;
            if (subsystem_filter.has_value() && node.subsystem != *subsystem_filter) continue;
            result.push_back(node);
        }
        
        return result;
    }
    
    // ========================================================================
    // Edge Management (Causal Relationships)
    // ========================================================================
    
    /**
     * Add an edge (causal relationship)
     */
    std::string addEdge(
        const std::string& source_id,
        const std::string& target_id,
        EdgeType type,
        double confidence = 1.0,
        const std::string& reasoning = "") {
        
        if (!isEnabled()) return "";
        
        GraphEdge edge;
        edge.source_id = source_id;
        edge.target_id = target_id;
        edge.type = type;
        edge.confidence = confidence;
        edge.reasoning = reasoning;
        
        // Auto-detect cross-layer transitions
        auto source_node = getNode(source_id);
        auto target_node = getNode(target_id);
        
        if (source_node && target_node &&
            m_config.auto_detect_cross_layer &&
            source_node->layer != target_node->layer) {
            
            if (isHigherLayer(source_node->layer, target_node->layer)) {
                edge.attributes["cross_layer"] = "down";
            } else {
                edge.attributes["cross_layer"] = "up";
            }
        }
        
        std::unique_lock lock(m_mutex);
        
        if (m_edges.size() >= m_config.max_edges) return "";
        
        m_edges[edge.id] = edge;
        m_adjacency[source_id].insert(target_id);
        
        return edge.id;
    }
    
    /**
     * Convenience: Record that A caused B
     */
    std::string recordCausation(
        const std::string& cause_id,
        const std::string& effect_id,
        double confidence = 0.8,
        const std::string& evidence = "") {
        
        return addEdge(cause_id, effect_id, EdgeType::CausedBy, confidence, evidence);
    }
    
    /**
     * Convenience: Record that A corrupted B's data
     */
    std::string recordCorruption(
        const std::string& corruptor_id,
        const std::string& victim_id,
        double confidence = 0.9,
        const std::string& details = "") {
        
        return addEdge(corruptor_id, victim_id, EdgeType::CorruptedBy, confidence, details);
    }
    
    // ========================================================================
    // Query Interface - Finding Root Causes
    // ========================================================================
    
    /**
     * Find all paths from potential causes to a symptom
     */
    std::vector<CausalPath> findPathsToSymptom(
        const std::string& symptom_id,
        int max_paths = 10,
        int max_length = -1) const {
        
        if (max_length < 0) max_length = m_config.max_path_length;
        
        std::shared_lock lock(m_mutex);
        
        std::vector<CausalPath> all_paths;
        
        // BFS from symptom backwards (following reverse edges)
        std::queue<std::pair<std::string, std::vector<GraphEdge>>> queue;
        queue.push({symptom_id, {}});
        
        std::set<std::string> visited;
        visited.insert(symptom_id);
        
        while (!queue.empty() && all_paths.size() < static_cast<size_t>(max_paths)) {
            auto [current_id, path_so_far] = queue.front();
            queue.pop();
            
            if (path_so_far.size() > static_cast<size_t>(max_length)) continue;
            
            // Look for nodes that have edges TO current_id (reverse traversal)
            for (const auto& [edge_id, edge] : m_edges) {
                if (edge.target_id == current_id && 
                    visited.find(edge.source_id) == visited.end()) {
                    
                    visited.insert(edge.source_id);
                    
                    CausalPath new_path;
                    new_path.edges = path_so_far;
                    new_path.edges.push_back(edge);
                    new_path.node_ids.push_back(edge.source_id);
                    new_path.node_ids.push_back(current_id);  // Will be deduplicated
                    
                    // Calculate path properties
                    calculatePathProperties(new_path);
                    
                    // Check if this could be a root cause
                    auto source_node_it = m_nodes.find(edge.source_id);
                    if (source_node_it != m_nodes.end()) {
                        const auto& source_node = source_node_it->second;
                        
                        // Good root cause candidates:
                        // - In a different layer than symptom
                        // - Is an error/corruption itself
                        // - Has no incoming edges (origin)
                        
                        auto symptom_node_it = m_nodes.find(symptom_id);
                        if (symptom_node_it != m_nodes.end()) {
                            if (source_node.layer != symptom_node_it->second.layer) {
                                new_path.cross_layer_transitions++;
                            }
                        }
                        
                        if (source_node.type == NodeType::Error ||
                            source_node.type == NodeType::Corruption ||
                            source_node.type == NodeType::Crash) {
                            all_paths.push_back(new_path);
                        }
                        
                        // Also add paths that reach origin nodes
                        bool has_incoming = false;
                        for (const auto& [e_id, e] : m_edges) {
                            if (e.target_id == edge.source_id) {
                                has_incoming = true;
                                break;
                            }
                        }
                        
                        if (!has_incoming && !path_so_far.empty()) {
                            // This is an origin node - good candidate
                            all_paths.push_back(new_path);
                        }
                    }
                    
                    // Continue searching from this node
                    queue.push({edge.source_id, new_path.edges});
                }
            }
        }
        
        // Sort by confidence/strength
        std::sort(all_paths.begin(), all_paths.end(),
            [](const CausalPath& a, const CausalPath& b) {
                return a.total_confidence > b.total_confidence;
            });
        
        return all_paths;
    }
    
    /**
     * Perform full root cause analysis for a symptom
     */
    RootCauseAnalysis analyzeRootCause(const std::string& symptom_id) {
        RootCauseAnalysis analysis;
        
        auto symptom_opt = getNode(symptom_id);
        if (!symptom_opt) {
            return analysis;  // Invalid symptom ID
        }
        
        analysis.symptom_node = *symptom_opt;
        
        // Find candidate paths
        analysis.candidate_paths = findPathsToSymptom(symptom_id, 50);
        
        // Score and rank each unique endpoint as a candidate
        std::map<std::string, RootCauseAnalysis::Candidate> candidates;
        
        for (const auto& path : analysis.candidate_paths) {
            if (path.node_ids.empty()) continue;
            
            std::string candidate_id = path.node_ids.front();  // Origin of path
            
            auto it = candidates.find(candidate_id);
            if (it == candidates.end()) {
                RootCauseAnalysis::Candidate cand;
                
                auto node_opt = getNode(candidate_id);
                if (node_opt) {
                    cand.node = *node_opt;
                    cand.path = path;
                    
                    // Score based on multiple factors
                    cand.overall_score = calculateCandidateScore(cand, analysis.symptom_node);
                    
                    candidates[candidate_id] = cand;
                }
            } else {
                // Keep best path for this candidate
                if (path.total_confidence > it->second.path.total_confidence) {
                    it->second.path = path;
                }
            }
        }
        
        // Move to vector and sort
        for (auto& [id, cand] : candidates) {
            analysis.ranked_candidates.push_back(std::move(cand));
        }
        
        std::sort(analysis.ranked_candidates.begin(), analysis.ranked_candidates.end(),
            std::greater<RootCauseAnalysis::Candidate>());
        
        // Set most likely cause
        if (!analysis.ranked_candidates.empty()) {
            analysis.most_likely_root_cause = analysis.ranked_candidates.front().node;
            analysis.most_likely_path = analysis.ranked_candidates.front().path;
            analysis.confidence_in_analysis = analysis.ranked_candidates.front().overall_score;
        }
        
        // Separate weak candidates as rejected
        for (const auto& cand : analysis.ranked_candidates) {
            if (cand.overall_score < 0.3) {
                analysis.rejected_candidates.push_back(cand);
                analysis.rejection_reasons.push_back(
                    "Low confidence score: " + std::to_string(cand.overall_score));
            }
        }
        
        return analysis;
    }
    
    /**
     * Find what a given node could have caused (forward traversal)
     */
    std::vector<GraphNode> findPossibleEffects(
        const std::string& cause_id,
        int max_depth = 3) const {
        
        std::shared_lock lock(m_mutex);
        
        std::vector<GraphNode> effects;
        std::set<std::string> visited;
        
        std::queue<std::pair<std::string, int>> queue;
        queue.push({cause_id, 0});
        visited.insert(cause_id);
        
        while (!queue.empty()) {
            auto [current_id, depth] = queue.front();
            queue.pop();
            
            if (depth >= max_depth) continue;
            
            // Follow outgoing edges
            auto adj_it = m_adjacency.find(current_id);
            if (adj_it != m_adjacency.end()) {
                for (const auto& target_id : adj_it->second) {
                    if (visited.find(target_id) == visited.end()) {
                        visited.insert(target_id);
                        
                        auto target_node_it = m_nodes.find(target_id);
                        if (target_node_it != m_nodes.end()) {
                            effects.push_back(target_node_it->second);
                        }
                        
                        queue.push({target_id, depth + 1});
                    }
                }
            }
        }
        
        return effects;
    }

private:
    Config m_config;
    std::atomic<bool> m_enabled;
    mutable std::shared_mutex m_mutex;
    
    std::map<std::string, GraphNode> m_nodes;
    std::map<std::string, GraphEdge> m_edges;
    std::map<std::string, std::set<std::string>> m_adjacency;  // source -> targets
    
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    bool isHigherLayer(EmulationLayer a, EmulationLayer b) const {
        // Guest is highest, Host is lowest
        static const std::map<EmulationLayer, int> layer_order = {
            {EmulationLayer::Guest, 10},
            {EmulationLayer::HLE, 9},
            {EmulationLayer::Kernel, 8},
            {EmulationLayer::FileSystem, 7},
            {EmulationLayer::Network, 7},
            {EmulationLayer::Audio, 6},
            {EmulationLayer::Input, 6},
            {EmulationLayer::GPU, 5},
            {EmulationLayer::Memory, 4},
            {EmulationLayer::CPU, 3},
            {EmulationLayer::Host, 2},
            {EmulationLayer::Diagnostic, 1}
        };
        
        auto it_a = layer_order.find(a);
        auto it_b = layer_order.find(b);
        
        if (it_a == layer_order.end() || it_b == layer_order.end()) return false;
        
        return it_a->second > it_b->second;
    }
    
    void calculatePathProperties(CausalPath& path) const {
        if (path.edges.empty()) {
            path.total_confidence = 0;
            path.strength = CausalPath::Strength::Strong;
            return;
        }
        
        // Confidence multiplies along path
        double conf = 1.0;
        for (const auto& edge : path.edges) {
            conf *= edge.confidence;
        }
        path.total_confidence = conf;
        
        // Count cross-layer transitions
        path.cross_layer_transitions = 0;
        EmulationLayer prev_layer = EmulationLayer::Diagnostic;
        
        for (const auto& edge : path.edges) {
            auto source = m_nodes.find(edge.source_id);
            if (source != m_nodes.end()) {
                if (source->second.layer != prev_layer && prev_layer != EmulationLayer::Diagnostic) {
                    path.cross_layer_transitions++;
                }
                prev_layer = source->second.layer;
            }
        }
        
        // Determine strength
        if (conf >= 0.8 && path.cross_layer_transitions >= 2) {
            path.strength = CausalPath::Strength::Strong;
        } else if (conf >= 0.5) {
            path.strength = CausalPath::Strength::Moderate;
        } else if (conf >= 0.2) {
            path.strength = CausalPath::Strength::Weak;
        } else {
            path.strength = CausalPath::Strength::VeryWeak;
        }
    }
    
    double calculateCandidateScore(const RootCauseAnalysis::Candidate& candidate, const GraphNode& symptom_node) const {
        double score = 0.0;
        
        // Base score from path confidence
        score += candidate.path.total_confidence * 0.4;
        
        // Bonus for cross-layer origin (different layer than symptom)
        if (candidate.node.layer != symptom_node.layer) {
            score += 0.2;
            
            // Extra bonus for lower-layer origins (more fundamental)
            if (isHigherLayer(symptom_node.layer, candidate.node.layer)) {
                score += 0.1;
            }
        }
        
        // Bonus for being an error/corruption/crash node
        if (candidate.node.type == NodeType::Error ||
            candidate.node.type == NodeType::Corruption ||
            candidate.node.type == NodeType::Crash) {
            score += 0.15;
        }
        
        // Bonus for path length (not too short, not too long)
        size_t path_len = candidate.path.edges.size();
        if (path_len >= 2 && path_len <= 5) {
            score += 0.1;
        }
        
        // Normalize to 0-1
        return std::min(score, 1.0);
    }
};

} // namespace graph
} // namespace prosper_debug
