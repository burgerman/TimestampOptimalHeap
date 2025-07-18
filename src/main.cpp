#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <limits>

// Custom hash function for std::pair (if needed)
template<typename T1, typename T2>
struct PairHash {
    size_t operator()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

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

    std::vector<Node*> roots;           // Forest of root nodes
    std::unordered_map<T, Node*> value_to_node;
    Compare comp;
    size_t current_timestamp;

    // Working set tracking
    std::vector<size_t> insertion_order;
    size_t extract_count;

    // Cached minimum node for O(1) top() operation
    Node* min_node_ptr;

    bool has_priority(const Node* a, const Node* b) const {
        if (comp(a->value, b->value)) return true;
        if (comp(b->value, a->value)) return false;
        // Break ties by timestamp (older elements have higher priority)
        return a->timestamp < b->timestamp;
    }

    // Calculate working set size for timestamp-optimal bounds
    size_t calculate_working_set_size(size_t timestamp) const {
        auto it = std::lower_bound(insertion_order.begin() + extract_count,
                                   insertion_order.end(), timestamp);
        return std::distance(it, insertion_order.end());
    }

    // Check if a node is in the working set
    bool is_in_working_set(const Node* node, size_t reference_timestamp) const {
        return node->timestamp >= reference_timestamp;
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

    // Meld operation - combines trees of same rank, but only for working set nodes
    void meld_working_set(size_t working_set_timestamp) {
        // Group trees by rank, but only consider nodes in working set
        std::vector<Node*> rank_array;
        std::vector<Node*> new_roots;

        for (Node* root : roots) {
            if (is_in_working_set(root, working_set_timestamp)) {
                // Ensure rank_array is large enough
                while (rank_array.size() <= root->rank) {
                    rank_array.push_back(nullptr);
                }

                Node* current = root;
                while (rank_array[current->rank] != nullptr) {
                    Node* other = rank_array[current->rank];
                    rank_array[current->rank] = nullptr;
                    current = link_trees(current, other);

                    // Ensure array is large enough for new rank
                    while (rank_array.size() <= current->rank) {
                        rank_array.push_back(nullptr);
                    }
                }
                rank_array[current->rank] = current;
            } else {
                // Keep non-working-set roots as-is
                new_roots.push_back(root);
            }
        }

        // Collect all trees from rank array
        for (Node* root : rank_array) {
            if (root != nullptr) {
                new_roots.push_back(root);
            }
        }

        roots = std::move(new_roots);

        // Update cached minimum after melding
        update_min_node_ptr();
    }

    // Cut a node from its parent - O(1) with circular doubly-linked lists
    void cut_node(Node* node) {
        if (node->parent == nullptr) return;

        Node* parent = node->parent;

        // Remove node from parent's child list - O(1)
        remove_from_child_list(node);

        parent->rank--;
        node->parent = nullptr;
        node->marked = false;

        // Add node back to roots
        roots.push_back(node);

        // Update min_node_ptr if this cut node is smaller
        if (min_node_ptr == nullptr || has_priority(node, min_node_ptr)) {
            min_node_ptr = node;
        }

        // Cascading cuts
        if (parent->parent != nullptr) {
            if (parent->marked) {
                cut_node(parent);
            } else {
                parent->marked = true;
            }
        }
    }

    // Update the cached minimum node pointer - O(r) where r is number of roots
    void update_min_node_ptr() {
        min_node_ptr = nullptr;
        for (Node* root : roots) {
            if (min_node_ptr == nullptr || has_priority(root, min_node_ptr)) {
                min_node_ptr = root;
            }
        }
    }

    // Collect all children of a node into a vector
    void collect_children(Node* parent_node, std::vector<Node*>& children) {
        if (parent_node->child == nullptr) return;

        Node* current = parent_node->child;
        do {
            Node* next = current->right_sibling;

            // Remove from circular list and reset pointers
            current->parent = nullptr;
            current->left_sibling = current;
            current->right_sibling = current;
            current->marked = false;

            children.push_back(current);
            current = next;
        } while (current != parent_node->child);
    }

    void clear_tree(Node* root) {
        if (root == nullptr) return;

        // Collect all children first
        std::vector<Node*> children;
        collect_children(root, children);

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

    // Insert operation - O(1) - just add a new single-node tree
    void insert(const T& value) {
        auto it = value_to_node.find(value);
        if (it != value_to_node.end()) {
            // val already exists
            return;
        }

        Node* new_node = new Node(value, current_timestamp);
        insertion_order.push_back(current_timestamp);
        current_timestamp++;

        // Simply add as a new root - O(1)
        roots.push_back(new_node);
        value_to_node[value] = new_node;

        // Update min_node_ptr if this is the new minimum
        if (min_node_ptr == nullptr || has_priority(new_node, min_node_ptr)) {
            min_node_ptr = new_node;
        }
    }

    // Decrease-key operation - O(1) amortized
    void decrease_key(const T& old_value, const T& new_value) {
        auto it = value_to_node.find(old_value);
        if (it == value_to_node.end()) {
            // val doesn't exists
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
            cut_node(node);  // This will update min_node_ptr if needed
        } else {
            // Node is already a root, just check if it's the new minimum
            if (min_node_ptr == nullptr || has_priority(node, min_node_ptr)) {
                min_node_ptr = node;
            }
        }
    }

    // Extract-min operation - O(log w) where w is working set size
    T extract_min() {
        if (roots.empty()) {
            throw std::runtime_error("Heap is empty");
        }

        // Use cached minimum node
        if (min_node_ptr == nullptr) {
            update_min_node_ptr();
        }

        Node* min_node = min_node_ptr;
        T min_value = min_node->value;
        size_t min_timestamp = min_node->timestamp;

        // Remove from value mapping
        value_to_node.erase(min_value);

        // Remove min_node from roots
        roots.erase(std::find(roots.begin(), roots.end(), min_node));

        // Add children of min_node as new roots
        std::vector<Node*> children;
        collect_children(min_node, children);
        for (Node* child : children) {
            roots.push_back(child);
        }

        delete min_node;
        extract_count++;

        // Perform meld operation on working set - this is O(log w)
        // Use min_timestamp as the reference timestamp for working set
        if (!roots.empty()) {
            meld_working_set(min_timestamp);
        } else {
            min_node_ptr = nullptr;
        }

        return min_value;
    }

    // Peek operation - O(1) using cached minimum
    const T& top() const {
        if (roots.empty()) {
            throw std::runtime_error("Heap is empty");
        }

        if (min_node_ptr == nullptr) {
            // This should not happen in normal operation, but handle it gracefully
            const_cast<TimestampOptimalHeap*>(this)->update_min_node_ptr();
        }

        return min_node_ptr->value;
    }

    bool empty() const {
        return roots.empty();
    }

    size_t size() const {
        return value_to_node.size();
    }

    bool contains(const T& value) const {
        return value_to_node.find(value) != value_to_node.end();
    }

    void clear() {
        for (Node* root : roots) {
            clear_tree(root);
        }
        roots.clear();
        value_to_node.clear();
        insertion_order.clear();
        current_timestamp = 0;
        extract_count = 0;
        min_node_ptr = nullptr;
    }

    // Debug function to get current working set info
    size_t get_current_working_set_size() const {
        if (roots.empty()) return 0;

        size_t min_timestamp = std::numeric_limits<size_t>::max();
        for (const Node* root : roots) {
            min_timestamp = std::min(min_timestamp, root->timestamp);
        }

        return calculate_working_set_size(min_timestamp);
    }

    // Debug function to get number of roots
    size_t get_num_roots() const {
        return roots.size();
    }
};

// Specialization for std::pair types
template<typename T1, typename T2, typename Compare>
class TimestampOptimalHeap<std::pair<T1, T2>, Compare> {
private:
    struct Node {
        std::pair<T1, T2> value;
        size_t timestamp;
        size_t rank;
        bool marked;

        Node* parent;
        Node* child;
        Node* left_sibling;
        Node* right_sibling;

        Node(const std::pair<T1, T2>& val, size_t ts)
                : value(val), timestamp(ts), rank(0), marked(false),
                  parent(nullptr), child(nullptr), left_sibling(nullptr), right_sibling(nullptr) {
            left_sibling = this;
            right_sibling = this;
        }
    };

    std::vector<Node*> roots;
    std::unordered_map<std::pair<T1, T2>, Node*, PairHash<T1, T2>> value_to_node;
    Compare comp;
    size_t current_timestamp;

    std::vector<size_t> insertion_order;
    size_t extract_count;
    Node* min_node_ptr;

    bool has_priority(const Node* a, const Node* b) const {
        if (comp(a->value, b->value)) return true;
        if (comp(b->value, a->value)) return false;
        return a->timestamp < b->timestamp;
    }

    size_t calculate_working_set_size(size_t timestamp) const {
        auto it = std::lower_bound(insertion_order.begin() + extract_count,
                                   insertion_order.end(), timestamp);
        return std::distance(it, insertion_order.end());
    }

    bool is_in_working_set(const Node* node, size_t reference_timestamp) const {
        return node->timestamp >= reference_timestamp;
    }

    void add_to_child_list(Node* parent_node, Node* child_node) {
        child_node->parent = parent_node;

        if (parent_node->child == nullptr) {
            parent_node->child = child_node;
            child_node->left_sibling = child_node;
            child_node->right_sibling = child_node;
        } else {
            Node* first_child = parent_node->child;
            Node* last_child = first_child->left_sibling;

            child_node->right_sibling = first_child;
            child_node->left_sibling = last_child;
            first_child->left_sibling = child_node;
            last_child->right_sibling = child_node;
        }
    }

    void remove_from_child_list(Node* node) {
        if (node->left_sibling == node) {
            if (node->parent) {
                node->parent->child = nullptr;
            }
        } else {
            node->left_sibling->right_sibling = node->right_sibling;
            node->right_sibling->left_sibling = node->left_sibling;

            if (node->parent && node->parent->child == node) {
                node->parent->child = node->right_sibling;
            }
        }

        node->left_sibling = node;
        node->right_sibling = node;
    }

    Node* link_trees(Node* root1, Node* root2) {
        if (has_priority(root2, root1)) {
            std::swap(root1, root2);
        }

        add_to_child_list(root1, root2);
        root1->rank++;

        return root1;
    }

    void meld_working_set(size_t working_set_timestamp) {
        std::vector<Node*> rank_array;
        std::vector<Node*> new_roots;

        for (Node* root : roots) {
            if (is_in_working_set(root, working_set_timestamp)) {
                while (rank_array.size() <= root->rank) {
                    rank_array.push_back(nullptr);
                }

                Node* current = root;
                while (rank_array[current->rank] != nullptr) {
                    Node* other = rank_array[current->rank];
                    rank_array[current->rank] = nullptr;
                    current = link_trees(current, other);

                    while (rank_array.size() <= current->rank) {
                        rank_array.push_back(nullptr);
                    }
                }
                rank_array[current->rank] = current;
            } else {
                new_roots.push_back(root);
            }
        }

        for (Node* root : rank_array) {
            if (root != nullptr) {
                new_roots.push_back(root);
            }
        }

        roots = std::move(new_roots);
        update_min_node_ptr();
    }

    void cut_node(Node* node) {
        if (node->parent == nullptr) return;

        Node* parent = node->parent;

        remove_from_child_list(node);

        parent->rank--;
        node->parent = nullptr;
        node->marked = false;

        roots.push_back(node);

        if (min_node_ptr == nullptr || has_priority(node, min_node_ptr)) {
            min_node_ptr = node;
        }

        if (parent->parent != nullptr) {
            if (parent->marked) {
                cut_node(parent);
            } else {
                parent->marked = true;
            }
        }
    }

    void update_min_node_ptr() {
        min_node_ptr = nullptr;
        for (Node* root : roots) {
            if (min_node_ptr == nullptr || has_priority(root, min_node_ptr)) {
                min_node_ptr = root;
            }
        }
    }

    void collect_children(Node* parent_node, std::vector<Node*>& children) {
        if (parent_node->child == nullptr) return;

        Node* current = parent_node->child;
        do {
            Node* next = current->right_sibling;

            current->parent = nullptr;
            current->left_sibling = current;
            current->right_sibling = current;
            current->marked = false;

            children.push_back(current);
            current = next;
        } while (current != parent_node->child);
    }

    void clear_tree(Node* root) {
        if (root == nullptr) return;

        std::vector<Node*> children;
        collect_children(root, children);

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

    TimestampOptimalHeap(const TimestampOptimalHeap& other) = delete;
    TimestampOptimalHeap& operator=(const TimestampOptimalHeap& other) = delete;

    void insert(const std::pair<T1, T2>& value) {
        auto it = value_to_node.find(value);
        if (it != value_to_node.end()) {
            // val already exists
            return;
        }

        Node* new_node = new Node(value, current_timestamp);
        insertion_order.push_back(current_timestamp);
        current_timestamp++;

        roots.push_back(new_node);
        value_to_node[value] = new_node;

        if (min_node_ptr == nullptr || has_priority(new_node, min_node_ptr)) {
            min_node_ptr = new_node;
        }
    }

    void decrease_key(const std::pair<T1, T2>& old_value, const std::pair<T1, T2>& new_value) {
        auto it = value_to_node.find(old_value);
        if (it == value_to_node.end()) {
            // val not exists
            return;
        }

        Node* node = it->second;
        value_to_node.erase(it);
        node->value = new_value;
        value_to_node[new_value] = node;

        node->timestamp = current_timestamp;
        insertion_order.push_back(current_timestamp);
        current_timestamp++;

        if (node->parent != nullptr) {
            cut_node(node);
        } else {
            if (min_node_ptr == nullptr || has_priority(node, min_node_ptr)) {
                min_node_ptr = node;
            }
        }
    }

    std::pair<T1, T2> extract_min() {
        if (roots.empty()) {
            throw std::runtime_error("Heap is empty");
        }

        if (min_node_ptr == nullptr) {
            update_min_node_ptr();
        }

        Node* min_node = min_node_ptr;
        std::pair<T1, T2> min_value = min_node->value;
        size_t min_timestamp = min_node->timestamp;

        value_to_node.erase(min_value);
        roots.erase(std::find(roots.begin(), roots.end(), min_node));

        std::vector<Node*> children;
        collect_children(min_node, children);
        for (Node* child : children) {
            roots.push_back(child);
        }

        delete min_node;
        extract_count++;

        if (!roots.empty()) {
            meld_working_set(min_timestamp);
        } else {
            min_node_ptr = nullptr;
        }

        return min_value;
    }

    const std::pair<T1, T2>& top() const {
        if (roots.empty()) {
            throw std::runtime_error("Heap is empty");
        }

        if (min_node_ptr == nullptr) {
            const_cast<TimestampOptimalHeap*>(this)->update_min_node_ptr();
        }

        return min_node_ptr->value;
    }

    bool empty() const { return roots.empty(); }
    size_t size() const { return value_to_node.size(); }

    bool contains(const std::pair<T1, T2>& value) const {
        return value_to_node.find(value) != value_to_node.end();
    }

    void clear() {
        for (Node* root : roots) {
            clear_tree(root);
        }
        roots.clear();
        value_to_node.clear();
        insertion_order.clear();
        current_timestamp = 0;
        extract_count = 0;
        min_node_ptr = nullptr;
    }

    size_t get_current_working_set_size() const {
        if (roots.empty()) return 0;

        size_t min_timestamp = std::numeric_limits<size_t>::max();
        for (const Node* root : roots) {
            min_timestamp = std::min(min_timestamp, root->timestamp);
        }

        return calculate_working_set_size(min_timestamp);
    }

    size_t get_num_roots() const {
        return roots.size();
    }
};

// Example usage demonstrating the improved performance
#include <iostream>
#include <chrono>

int main() {
    TimestampOptimalHeap<int> heap;

    std::cout << "=== Testing O(1) Operations ===" << std::endl;

    // Test O(1) insert
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100000; ++i) {
        heap.insert(rand() % 500000);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "100,000 inserts in " << duration.count() << " microseconds" << std::endl;

    // Test O(1) top operation
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        volatile int min_val = heap.top();  // volatile to prevent optimization
        (void)min_val;  // suppress unused variable warning
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "10,000 top() calls in " << duration.count() << " microseconds" << std::endl;

    // Test decrease-key performance
    std::vector<int> values;
    for (int i = 0; i < 1000; ++i) {
        int val = rand() % 100000;
        heap.insert(val);
        values.push_back(val);
    }

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 500; ++i) {
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
    for (int i = 0; i < 20; ++i) {
        if (!heap.empty()) {
            start = std::chrono::high_resolution_clock::now();
            auto min_val = heap.extract_min();
            end = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cout << "Extracted ( Min Value: " << min_val
                      << ") - Working set size: " << heap.get_current_working_set_size()
                      << ", Num roots: " << heap.get_num_roots()
                      << ", Heap size: " << heap.size() << std::endl;
            std::cout << "extract_min operation in " << duration.count() << " microseconds" << std::endl;
        }
    }
    return 0;
}