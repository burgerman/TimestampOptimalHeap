#ifndef TIMESTAMPOPTIMALHEAP_DIJKSTRA_H
#define TIMESTAMPOPTIMALHEAP_DIJKSTRA_H
#include "Graph.h"
#include "TimestampOptimalHeap.h"
#include <unordered_set>
#include <limits>
#include <iostream>
template<typename NodeId = int, typename Weight = double>
// Stores the results of a Dijkstra's algorithm run, including distances and paths.
class DijkstraResult {
public:
    // Maps each reachable node to its shortest distance from the source.
    std::unordered_map<NodeId, Weight> distances;
    // Maps each node to its predecessor in the shortest path tree.
    std::unordered_map<NodeId, NodeId> predecessors;
    // The source node for the Dijkstra run.
    NodeId source;
    // Flag indicating if the target was found (for single-target searches).
    bool found_target;
    // The target node for the Dijkstra run (if applicable).
    NodeId target;

    DijkstraResult(NodeId src) : source(src), found_target(false) {}

    // Reconstructs and returns the shortest path from the source to a given destination node.
    // Returns an empty vector if the destination is not reachable.
    std::vector<NodeId> get_path_to(NodeId dest) const {
        std::vector<NodeId> path;

        // Check if destination is reachable
        if (distances.find(dest) == distances.end()) {
            return path; // Empty path means unreachable
        }

        // Reconstruct path backwards from destination to source
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

    // Returns the shortest distance to a specific destination node.
    // Returns infinity if the node is not reachable.
    Weight get_distance_to(NodeId dest) const {
        auto it = distances.find(dest);
        return (it != distances.end()) ? it->second : std::numeric_limits<Weight>::infinity();
    }

    // Checks if a given destination node is reachable from the source.
    bool is_reachable(NodeId dest) const {
        return distances.find(dest) != distances.end();
    }
};

template<typename NodeId = int, typename Weight = double>
// Represents an entry in the priority queue for Dijkstra's algorithm.
// It contains the node ID and its current known shortest distance from the source.
struct HeapEntry {
    Weight distance;
    NodeId node;

    HeapEntry(Weight d, NodeId n) : distance(d), node(n) {}

    // Comparison operator for the min-heap. It prioritizes smaller distances.
    // Node IDs are used as a tie-breaker to ensure a consistent ordering.
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
    // Custom hash function for HeapEntry to allow its use in unordered maps/sets.
    template<typename NodeId, typename Weight>
    struct hash<HeapEntry<NodeId, Weight>> {
        // Better avalanche effect - each XOR and multiply ops spreads bit changes throughout the result
        // Proven constants - FNV constants are mathematically chosen to minimize collisions
        // Sequential mixing - processes each hash value separately, preventing interference
        // More thorough mixing - the multiply ops provide much better bit distribution than simple shifts
    std::size_t operator()(const HeapEntry<NodeId, Weight>& entry) const {
        constexpr std::size_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
        constexpr std::size_t FNV_PRIME = 1099511628211ULL;
        // Better hash combining, using both low and high bits of h2
        // Lower potential for hash collisions
        std::size_t h1 = std::hash<Weight>()(entry.distance);
        std::size_t h2 = std::hash<NodeId>()(entry.node);

        std::size_t result = FNV_OFFSET_BASIS;
        result ^= h1;
        result *= FNV_PRIME;
        result ^= h2;
        result *= FNV_PRIME;
        return result;
    }
    };
}

template<typename NodeId = int, typename Weight = double>
// Provides an optimized implementation of Dijkstra's algorithm using a TimestampOptimalHeap.
class OptimalDijkstra {

public:
    using Entry = HeapEntry<NodeId, Weight>;
    // Computes the shortest paths from a single source node to all other reachable nodes in the graph.
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
            if (visited.count(current)) {
                continue;
            }

            visited.insert(current);

            // Process all neighbors
            for (const auto& edge : graph.get_neighbors(current)) {
                NodeId neighbor = edge.to;
                Weight edge_weight = edge.weight;
                Weight new_distance = current_dist + edge_weight;

                // Skip if neighbor already processed
                if (visited.count(neighbor)) {
                    continue;
                }

                Weight old_distance = result.get_distance_to(neighbor);

                if (new_distance < old_distance) {
                    // Update distance and predecessor
                    result.distances[neighbor] = new_distance;
                    result.predecessors[neighbor] = current;

                    // Add/update in priority queue
                    if (old_distance != std::numeric_limits<Weight>::infinity()) {
                        // Use decrease-key operation
                        pq.decrease_key(Entry(old_distance, neighbor), Entry(new_distance, neighbor));
                    } else {
                        // Insert new entry
                        pq.insert(Entry(new_distance, neighbor));
                    }
                }
            }
        }

        return result;
    }

    // Computes the shortest path from a source to a specific target node.
    // This version includes an early exit optimization to stop once the target is reached.
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
            if (visited.count(current)) {
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
                if (visited.count(neighbor)) {
                    continue;
                }

                Weight old_distance = result.get_distance_to(neighbor);

                if (new_distance < old_distance) {
                    result.distances[neighbor] = new_distance;
                    result.predecessors[neighbor] = current;

                    if (old_distance != std::numeric_limits<Weight>::infinity()) {
                        pq.decrease_key(Entry(old_distance, neighbor), Entry(new_distance, neighbor));
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
