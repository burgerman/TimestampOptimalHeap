#ifndef TIMESTAMPOPTIMALHEAP_DIJKSTRA_H
#define TIMESTAMPOPTIMALHEAP_DIJKSTRA_H
#include "Graph.h"
#include "TimestampOptimalHeap.h"
#include <unordered_set>
#include <limits>
#include <iostream>
template<typename NodeId = int, typename Weight = double>
class DijkstraResult {
public:
    std::unordered_map<NodeId, Weight> distances;
    std::unordered_map<NodeId, NodeId> predecessors;
    NodeId source;
    bool found_target;
    NodeId target;

    DijkstraResult(NodeId src) : source(src), found_target(false) {}

    // Get shortest path to a specific target
    std::vector<NodeId> get_path_to(NodeId dest) const {
        std::vector<NodeId> path;

        // Check if destination is reachable
        if (distances.find(dest) == distances.end()) {
            return path; // Empty path means unreachable
        }

        // Reconstruct path backwards
        NodeId current = dest;
        while (current != source) {
            path.push_back(current);
            auto it = predecessors.find(current);
            if (it == predecessors.end()) {
                return std::vector<NodeId>(); // Path broken, return empty
            }
            current = it->second;
        }
        path.push_back(source);

        // Reverse to get path from source to destination
        std::reverse(path.begin(), path.end());
        return path;
    }

    // Get distance to a specific node
    Weight get_distance_to(NodeId dest) const {
        auto it = distances.find(dest);
        return (it != distances.end()) ? it->second : std::numeric_limits<Weight>::infinity();
    }

    // Check if a node is reachable
    bool is_reachable(NodeId dest) const {
        return distances.find(dest) != distances.end();
    }
};

template<typename NodeId = int, typename Weight = double>
// Priority queue entry for the heap
struct HeapEntry {
    Weight distance;
    NodeId node;

    HeapEntry(Weight d, NodeId n) : distance(d), node(n) {}

    // For the heap's comparison - we want min-heap behavior
    bool operator<(const HeapEntry& other) const {
        if (distance != other.distance) {
            return distance < other.distance;
        }
        return node < other.node; // Tie-breaking by node ID
    }

    bool operator==(const HeapEntry& other) const {
        return distance == other.distance && node == other.node;
    }
};


namespace std {
    template<typename NodeId, typename Weight>
    struct hash<HeapEntry<NodeId, Weight>> {
    std::size_t operator()(const HeapEntry<NodeId, Weight>& entry) const {
        return std::hash<Weight>()(entry.distance) ^
               (std::hash<NodeId>()(entry.node) << 1);
    }
};
}

template<typename NodeId = int, typename Weight = double>
class OptimalDijkstra {

public:
    using Entry = HeapEntry<NodeId, Weight>;
    // Single-source shortest paths to all reachable nodes
    static DijkstraResult<NodeId, Weight> shortest_paths(
            const Graph<NodeId, Weight>& graph,
            NodeId source) {

        DijkstraResult<NodeId, Weight> result(source);
        TimestampOptimalHeap<Entry> pq;
        std::unordered_set<NodeId> visited;

        // Initialize source
        result.distances[source] = Weight(0);
        pq.insert(Entry(Weight(0), source));

        while (!pq.empty()) {
            // Extract minimum distance node
            Entry current_entry = pq.extract_min();
            NodeId current = current_entry.node;
            Weight current_dist = current_entry.distance;

            // Skip if already processed (can happen due to decrease-key operations)
            if (visited.find(current) != visited.end()) {
                continue;
            }

            visited.insert(current);

            // Process all neighbors
            for (const auto& edge : graph.get_neighbors(current)) {
                NodeId neighbor = edge.to;
                Weight edge_weight = edge.weight;
                Weight new_distance = current_dist + edge_weight;

                // Skip if neighbor already processed
                if (visited.find(neighbor) != visited.end()) {
                    continue;
                }

                bool should_update = false;
                Weight old_distance = std::numeric_limits<Weight>::infinity();

                auto dist_it = result.distances.find(neighbor);
                if (dist_it == result.distances.end()) {
                    // First time seeing this neighbor
                    should_update = true;
                } else {
                    old_distance = dist_it->second;
                    if (new_distance < old_distance) {
                        should_update = true;
                    }
                }

                if (should_update) {
                    // Update distance and predecessor
                    result.distances[neighbor] = new_distance;
                    result.predecessors[neighbor] = current;

                    // Add/update in priority queue
                    if (old_distance != std::numeric_limits<Weight>::infinity()) {
                        // Use decrease-key operation
                        Entry old_entry(old_distance, neighbor);
                        Entry new_entry(new_distance, neighbor);
                        pq.decrease_key(old_entry, new_entry);
                    } else {
                        // Insert new entry
                        pq.insert(Entry(new_distance, neighbor));
                    }
                }
            }
        }

        return result;
    }

    // Single-source, single-target shortest path (early termination)
    static DijkstraResult<NodeId, Weight> shortest_path(
            const Graph<NodeId, Weight>& graph,
            NodeId source,
            NodeId target) {

        DijkstraResult<NodeId, Weight> result(source);
        result.target = target;

        if (source == target) {
            result.distances[source] = Weight(0);
            result.found_target = true;
            return result;
        }

        TimestampOptimalHeap<Entry> pq;
        std::unordered_set<NodeId> visited;

        // Initialize source
        result.distances[source] = Weight(0);
        pq.insert(Entry(Weight(0), source));

        while (!pq.empty()) {
            // Extract minimum distance node
            Entry current_entry = pq.extract_min();
            NodeId current = current_entry.node;
            Weight current_dist = current_entry.distance;

            // Skip if already processed
            if (visited.find(current) != visited.end()) {
                continue;
            }

            visited.insert(current);

            // Early termination if we reached the target
            if (current == target) {
                result.found_target = true;
                return result;
            }

            // Process all neighbors
            for (const auto& edge : graph.get_neighbors(current)) {
                NodeId neighbor = edge.to;
                Weight edge_weight = edge.weight;
                Weight new_distance = current_dist + edge_weight;

                // Skip if neighbor already processed
                if (visited.find(neighbor) != visited.end()) {
                    continue;
                }

                bool should_update = false;
                Weight old_distance = std::numeric_limits<Weight>::infinity();

                auto dist_it = result.distances.find(neighbor);
                if (dist_it == result.distances.end()) {
                    should_update = true;
                } else {
                    old_distance = dist_it->second;
                    if (new_distance < old_distance) {
                        should_update = true;
                    }
                }

                if (should_update) {
                    result.distances[neighbor] = new_distance;
                    result.predecessors[neighbor] = current;

                    if (old_distance != std::numeric_limits<Weight>::infinity()) {
                        Entry old_entry(old_distance, neighbor);
                        Entry new_entry(new_distance, neighbor);
                        pq.decrease_key(old_entry, new_entry);
                    } else {
                        pq.insert(Entry(new_distance, neighbor));
                    }
                }
            }
        }

        return result;
    }
};
#endif //TIMESTAMPOPTIMALHEAP_DIJKSTRA_H
