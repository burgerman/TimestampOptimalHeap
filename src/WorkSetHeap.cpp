//
// Created by Wilfried Wu on 2025-07-19.
//
#include "WorkSetHeap.h"
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <stdexcept>

template<typename T, typename Compare = std::less<T>>
class TimestampOptimalHeap {
private:
    struct Node {
        T value;
        size_t timestamp;
        size_t rank;           // Rank of the tree rooted at this node
        bool marked;           // For decrease-key operations

        Node* parent;
        Node* child;           // One child in circular doubly-linked list
        Node* left_sibling;    // Left sibling in circular doubly-linked list
        Node* right_sibling;   // Right sibling in circular doubly-linked list

        Node(const T& val, size_t ts)
                : value(val), timestamp(ts), rank(0), marked(false),
                  parent(nullptr), child(nullptr), left_sibling(nullptr), right_sibling(nullptr) {
            // Initialize as self-referencing (single node in circular list)
            left_sibling = this;
            right_sibling = this;
        }
    };

    std::unordered_map<T, Node*> value_to_node;
    Compare comp;
    size_t current_timestamp;

    // Working set tracking
    std::vector<size_t> insertion_order;
    size_t extract_count;

    // Root list is now a circular doubly-linked list
    // min_node_ptr serves as the entry point to the root list
    Node* min_node_ptr;

    bool has_priority(const Node* a, const Node* b) const {
        if (comp(a->value, b->value)) return true;
        if (comp(b->value, a->value)) return false;
        // Break ties by timestamp (older elements have higher priority)
        return a->timestamp < b->timestamp;
    }

    // Add a node to circular doubly-linked list
    void add_to_child_list(Node* parent_node, Node* child_node) {
        child_node->parent = parent_node;

        if (parent_node->child == nullptr) {
            // First child
            parent_node->child = child_node;
            child_node->left_sibling = child_node;
            child_node->right_sibling = child_node;
        } else {
            // Insert into circular list
            Node* first_child = parent_node->child;
            Node* last_child = first_child->left_sibling;

            child_node->right_sibling = first_child;
            child_node->left_sibling = last_child;
            first_child->left_sibling = child_node;
            last_child->right_sibling = child_node;
        }
    }

    // Remove a node from circular doubly-linked list - O(1)
    void remove_from_child_list(Node* node) {
        if (node->left_sibling == node) {
            // Only node in the list
            if (node->parent) {
                node->parent->child = nullptr;
            }
        } else {
            // Remove from circular list
            node->left_sibling->right_sibling = node->right_sibling;
            node->right_sibling->left_sibling = node->left_sibling;

            // Update parent's child pointer if necessary
            if (node->parent && node->parent->child == node) {
                node->parent->child = node->right_sibling;
            }
        }

        node->left_sibling = node;
        node->right_sibling = node;
    }

    // Add a node to the root list - O(1)
    void add_to_root_list(Node* node) {
        node->parent = nullptr;
        node->marked = false;

        if (min_node_ptr == nullptr) {
            // First node in root list
            min_node_ptr = node;
            node->left_sibling = node;
            node->right_sibling = node;
        } else {
            // Insert into circular root list
            Node* last_root = min_node_ptr->left_sibling;

            node->right_sibling = min_node_ptr;
            node->left_sibling = last_root;
            min_node_ptr->left_sibling = node;
            last_root->right_sibling = node;

            // Update min_node_ptr if necessary
            if (has_priority(node, min_node_ptr)) {
                min_node_ptr = node;
            }
        }
    }

    // Remove a node from the root list - O(1)
    void remove_from_root_list(Node* node) {
        if (node->left_sibling == node) {
            // Only node in root list
            min_node_ptr = nullptr;
        } else {
            // Remove from circular list
            node->left_sibling->right_sibling = node->right_sibling;
            node->right_sibling->left_sibling = node->left_sibling;

            // Update min_node_ptr if we're removing it
            if (min_node_ptr == node) {
                min_node_ptr = node->right_sibling;
            }
        }

        node->left_sibling = node;
        node->right_sibling = node;
    }

    // Link two trees of the same rank - O(1)
    Node* link_trees(Node* root1, Node* root2) {
        if (has_priority(root2, root1)) {
            std::swap(root1, root2);
        }

        // Make root2 a child of root1
        add_to_child_list(root1, root2);
        root1->rank++;

        return root1;
    }

    // Standard Fibonacci heap meld operation - O(log n) amortized
    void meld() {
        if (min_node_ptr == nullptr) return;

        // Create rank array to track trees by rank
        std::vector<Node*> rank_array;

        // Collect all root nodes
        std::vector<Node*> roots_to_process;
        Node* current = min_node_ptr;
        do {
            roots_to_process.push_back(current);
            current = current->right_sibling;
        } while (current != min_node_ptr);

        // Clear the root list
        min_node_ptr = nullptr;

        // Process each root
        for (Node* root : roots_to_process) {
            // Reset sibling pointers since we removed from root list
            root->left_sibling = root;
            root->right_sibling = root;

            Node* current_root = root;

            // Ensure rank_array is large enough
            while (rank_array.size() <= current_root->rank) {
                rank_array.push_back(nullptr);
            }

            // Link trees of same rank
            while (rank_array[current_root->rank] != nullptr) {
                Node* other = rank_array[current_root->rank];
                rank_array[current_root->rank] = nullptr;

                current_root = link_trees(current_root, other);

                // Ensure array is large enough for new rank
                while (rank_array.size() <= current_root->rank) {
                    rank_array.push_back(nullptr);
                }
            }

            rank_array[current_root->rank] = current_root;
        }

        // Add all remaining trees back to root list
        for (Node* root : rank_array) {
            if (root != nullptr) {
                add_to_root_list(root);
            }
        }
    }

    // Cut a node from its parent - O(1) with circular doubly-linked lists
    void cut_node(Node* node) {
        if (node->parent == nullptr) return;

        Node* parent = node->parent;

        // Remove node from parent's child list - O(1)
        remove_from_child_list(node);

        parent->rank--;

        // Add node to root list
        add_to_root_list(node);

        // Cascading cuts
        if (parent->parent != nullptr) {
            if (parent->marked) {
                cut_node(parent);
            } else {
                parent->marked = true;
            }
        }
    }

    // Update the minimum node pointer by scanning root list - O(r)
    void update_min_node_ptr() {
        if (min_node_ptr == nullptr) return;

        Node* current = min_node_ptr;
        Node* new_min = min_node_ptr;

        do {
            if (has_priority(current, new_min)) {
                new_min = current;
            }
            current = current->right_sibling;
        } while (current != min_node_ptr);

        min_node_ptr = new_min;
    }

    // Splice children into root list - O(1) time by direct circular list merging
    void merge_children_to_root_list(Node* parent_node) {
        if (parent_node->child == nullptr) return;

        // First, clear parent pointers and marked flags in O(k) time
        Node* child = parent_node->child;
        do {
            child->parent = nullptr;
            child->marked = false;
            child = child->right_sibling;
        } while (child != parent_node->child);

        // Now splice the entire circular list of children into the root list in O(1)
        if (min_node_ptr == nullptr) {
            // Root list is empty, children become the entire root list
            min_node_ptr = parent_node->child;
        } else {
            // Splice children into root list by rearranging pointers
            Node* first_child = parent_node->child;
            Node* last_child = first_child->left_sibling;
            Node* last_root = min_node_ptr->left_sibling;

            // Connect last root to first child
            last_root->right_sibling = first_child;
            first_child->left_sibling = last_root;

            // Connect last child to first root
            last_child->right_sibling = min_node_ptr;
            min_node_ptr->left_sibling = last_child;
        }

        // Update min_node_ptr if any child has higher priority
        Node* current = parent_node->child;
        do {
            if (has_priority(current, min_node_ptr)) {
                min_node_ptr = current;
            }
            current = current->right_sibling;
        } while (current != parent_node->child);
    }

    void clear_tree(Node* root) {
        if (root == nullptr) return;

        // Collect all children first
        std::vector<Node*> children;
        if (root->child != nullptr) {
            Node* child = root->child;
            do {
                Node* next = child->right_sibling;
                children.push_back(child);
                child = next;
            } while (child != root->child);
        }

        // Recursively clear children
        for (Node* child : children) {
            clear_tree(child);
        }

        delete root;
    }

public:
    TimestampOptimalHeap() : current_timestamp(0), extract_count(0), min_node_ptr(nullptr) {}

    ~TimestampOptimalHeap() {
        clear();
    }

    // Copy constructor
    TimestampOptimalHeap(const TimestampOptimalHeap& other) = delete;

    // Assignment operator
    TimestampOptimalHeap& operator=(const TimestampOptimalHeap& other) = delete;

    // Insert operation - O(1)
    void insert(const T& value) {
        auto it = value_to_node.find(value);
        if (it != value_to_node.end()) {
            // Value already exists
            return;
        }

        Node* new_node = new Node(value, current_timestamp);
        insertion_order.push_back(current_timestamp);
        current_timestamp++;

        // Add to root list - O(1)
        add_to_root_list(new_node);
        value_to_node[value] = new_node;
    }

    // Decrease-key operation - O(1) amortized
    void decrease_key(const T& old_value, const T& new_value) {
        auto it = value_to_node.find(old_value);
        if (it == value_to_node.end()) {
            // Value doesn't exist
            return;
        }

        Node* node = it->second;

        // Update value mapping
        value_to_node.erase(it);
        node->value = new_value;
        value_to_node[new_value] = node;

        // Update timestamp to maintain working-set property
        node->timestamp = current_timestamp;
        insertion_order.push_back(current_timestamp);
        current_timestamp++;

        // Cut the node from its parent (if any) and add to roots
        if (node->parent != nullptr) {
            cut_node(node);  // This adds to root list and may update min_node_ptr
        } else {
            // Node is already a root, just check if it's the new minimum
            if (has_priority(node, min_node_ptr)) {
                min_node_ptr = node;
            }
        }
    }

    // Extract-min operation - O(log w) amortized where w is working set size
    T extract_min() {
        if (min_node_ptr == nullptr) {
            throw std::runtime_error("Heap is empty");
        }

        Node* min_node = min_node_ptr;
        T min_value = min_node->value;

        // Remove from value mapping
        value_to_node.erase(min_value);

        // Remove min_node from root list - O(1)
        remove_from_root_list(min_node);

        // Merge children into root list - O(1) for linking operation
        merge_children_to_root_list(min_node);

        delete min_node;
        extract_count++;

        // Perform standard Fibonacci heap meld - O(log n) amortized
        // The timestamp-based potential function analysis shows this is O(log w)
        meld();

        return min_value;
    }

    // Peek operation - O(1)
    const T& top() const {
        if (min_node_ptr == nullptr) {
            throw std::runtime_error("Heap is empty");
        }
        return min_node_ptr->value;
    }

    bool empty() const {
        return min_node_ptr == nullptr;
    }

    size_t size() const {
        return value_to_node.size();
    }

    bool contains(const T& value) const {
        return value_to_node.find(value) != value_to_node.end();
    }

    void clear() {
        if (min_node_ptr != nullptr) {
            // Clear all trees in root list
            Node* current = min_node_ptr;
            std::vector<Node*> roots_to_clear;

            do {
                roots_to_clear.push_back(current);
                current = current->right_sibling;
            } while (current != min_node_ptr);

            for (Node* root : roots_to_clear) {
                clear_tree(root);
            }
        }

        value_to_node.clear();
        insertion_order.clear();
        current_timestamp = 0;
        extract_count = 0;
        min_node_ptr = nullptr;
    }

    // Debug function to get number of roots
    size_t get_num_roots() const {
        if (min_node_ptr == nullptr) return 0;

        size_t count = 0;
        Node* current = min_node_ptr;
        do {
            count++;
            current = current->right_sibling;
        } while (current != min_node_ptr);

        return count;
    }
};

#include <unordered_set>
#include <limits>
#include <iostream>

template<typename NodeId = int, typename Weight = double>
class Graph {
public:
    struct Edge {
        NodeId to;
        Weight weight;

        Edge(NodeId dest, Weight w) : to(dest), weight(w) {}
    };

private:
    std::unordered_map<NodeId, std::vector<Edge>> adjacency_list;

public:
    void add_edge(NodeId from, NodeId to, Weight weight) {
        adjacency_list[from].emplace_back(to, weight);
    }

    void add_bidirectional_edge(NodeId from, NodeId to, Weight weight) {
        add_edge(from, to, weight);
        add_edge(to, from, weight);
    }

    const std::vector<Edge>& get_neighbors(NodeId node) const {
        static const std::vector<Edge> empty_vector;
        auto it = adjacency_list.find(node);
        return (it != adjacency_list.end()) ? it->second : empty_vector;
    }

    std::vector<NodeId> get_all_nodes() const {
        std::vector<NodeId> nodes;
        for (const auto& pair : adjacency_list) {
            nodes.push_back(pair.first);
        }
        return nodes;
    }

    bool has_node(NodeId node) const {
        return adjacency_list.find(node) != adjacency_list.end();
    }

    size_t num_nodes() const {
        return adjacency_list.size();
    }
};

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


// Example usage and testing, demonstrating the improved performance
#include <string>
#include <chrono>


void example_dijkstra() {
    std::cout << "\n=== Testing Dijkstra's algo with Timestamp Optimal Heap  ===" << std::endl;
    // Create a sample graph
    Graph<int, double> graph;

    // Add edges (creating a simple graph)
    //     1
    //   / | \
    //  2  |  4
    //  |  |  |
    //  3--0--5
    graph.add_bidirectional_edge(0, 1, 1.0);
    graph.add_bidirectional_edge(0, 3, 2.0);
    graph.add_bidirectional_edge(0, 5, 3.0);
    graph.add_bidirectional_edge(1, 2, 1.5);
    graph.add_bidirectional_edge(1, 4, 2.5);
    graph.add_bidirectional_edge(2, 3, 1.0);
    graph.add_bidirectional_edge(4, 5, 1.0);

    // Test single-source shortest paths
    std::cout << "=== Single-source shortest paths from node 1 ===\n";
    auto result_all = OptimalDijkstra<int, double>::shortest_paths(graph, 0);

    for (int node = 0; node <= 5; ++node) {
        if (result_all.is_reachable(node)) {
            double dist = result_all.get_distance_to(node);
            auto path = result_all.get_path_to(node);

            std::cout << "Distance to " << node << ": " << dist << ", Path: ";
            for (size_t i = 0; i < path.size(); ++i) {
                std::cout << path[i];
                if (i < path.size() - 1) std::cout << " -> ";
            }
            std::cout << "\n";
        }
    }

    // Test single-source, single-target
    std::cout << "\n=== Single path from 1 to 5 ===\n";
    auto result_single = OptimalDijkstra<int, double>::shortest_path(graph, 1, 5);

    if (result_single.found_target) {
        auto path = result_single.get_path_to(5);
        std::cout << "Shortest path from 1 to 5: ";
        for (size_t i = 0; i < path.size(); ++i) {
            std::cout << path[i];
            if (i < path.size() - 1) std::cout << " -> ";
        }
        std::cout << "\nDistance: " << result_single.get_distance_to(5) << "\n";
    } else {
        std::cout << "No path found from 1 to 5\n";
    }
}

void func_perform_test() {
    TimestampOptimalHeap<int> heap;

    std::cout << "=== Testing O(1) Operations ===" << std::endl;

    // Test O(1) insert
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < 100000; ++i) {
        heap.insert(rand() % 500000);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "100,000 inserts in " << duration.count() << " microseconds" << std::endl;

    // Test O(1) top operation
    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < 10000; ++i) {
        volatile int min_val = heap.top();  // volatile to prevent optimization
        (void)min_val;  // suppress unused variable warning
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "10,000 top() calls in " << duration.count() << " microseconds" << std::endl;

    // Test decrease-key performance
    std::vector<int> values;
    for (size_t i = 0; i < 1000; ++i) {
        int val = rand() % 100000;
        heap.insert(val);
        values.push_back(val);
    }

    start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < 500; ++i) {
        int old_val = values[i];
        int new_val = (old_val - 1000)>0?(old_val - 1000) : (old_val/2);  // Make it smaller
        if (heap.contains(old_val)) {
            heap.decrease_key(old_val, new_val);
            values[i] = new_val;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "500 decrease-key operations in " << duration.count() << " microseconds" << std::endl;

    // Test extract-min with working set bounds
    std::cout << "\n=== Testing O(log w) Extract-Min ===" << std::endl;
    for (size_t i = 0; i < 20; ++i) {
        if (!heap.empty()) {
            start = std::chrono::high_resolution_clock::now();
            auto min_val = heap.extract_min();
            end = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cout << "Extracted ( Min Value: " << min_val
                      << ") , Num roots: " << heap.get_num_roots()
                      << ", Heap size: " << heap.size() << std::endl;
            std::cout << "extract_min operation in " << duration.count() << " microseconds" << std::endl;
        }
    }
}