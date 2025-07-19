#ifndef TIMESTAMPOPTIMALHEAP_TIMESTAMPOPTIMALHEAP_H
#define TIMESTAMPOPTIMALHEAP_TIMESTAMPOPTIMALHEAP_H
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <cstddef>
#include <memory>

template<typename T, typename Compare = std::less<T>>
// Implements a Timestamp-based Optimal Heap, a variant of a Fibonacci Heap.
// This data structure is optimized for scenarios where the working set of elements
// is much smaller than the total number of elements, providing O(log w) amortized
// time for extract-min, where 'w' is the working set size.
class TimestampOptimalHeap {
private:
    // Represents a node within the heap's internal forest of trees.
    struct Node {
        T value;
        std::size_t timestamp; // Timestamp of the last operation (insert/decrease-key).
        std::size_t rank;      // Number of children, used for tree linking.
        bool marked;           // Flag for cascading cuts in decrease-key.

        Node* parent;
        Node* child;           // Pointer to one of its children.
        Node* left_sibling;    // Sibling pointers for a circular doubly-linked list.
        Node* right_sibling;

        Node(const T& val, std::size_t ts)
                : value(val), timestamp(ts), rank(0), marked(false),
                  parent(nullptr), child(nullptr), left_sibling(this), right_sibling(this) {
        }
        // Enable move version of constructor for optimization
        Node(T&& val, std::size_t ts)
                : value(std::move(val)), timestamp(ts), rank(0), marked(false),
                  parent(nullptr), child(nullptr), left_sibling(this), right_sibling(this) {
        }
    };

    struct HeapStatistics {
        std::size_t total_operations = 0;
        std::size_t max_working_set_size = 0;
        std::size_t consolidation_count = 0;
    };

    // Maps values to their corresponding nodes for O(1) average time access.
    std::unordered_map<T, Node*> value_to_node;
    Compare comp; // The comparison function to establish heap order.
    std::size_t current_timestamp; // Monotonically increasing timestamp counter.

    // Data structures for tracking the working set.
    std::vector<std::size_t> insertion_order;
    std::size_t extract_count;

    // The root list is a circular doubly-linked list.
    // min_node_ptr points to the node with the highest priority (minimum value).
    Node* min_node_ptr;

    // Calculate based on insertion_order and extract_count
    std::size_t current_working_set_size() const noexcept{
        return insertion_order.size() - extract_count;
    }

    // Determines if node 'a' has higher priority than node 'b'.
    // Tie-breaks using the timestamp to favor older elements.
    bool has_priority(const Node* a, const Node* b) const noexcept{
        if (comp(a->value, b->value)) return true;
        if (comp(b->value, a->value)) return false;
        return a->timestamp < b->timestamp;
    }

    // Adds a child node to a parent's circular doubly-linked list of children.
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

    // Removes a node from its sibling list in O(1) time.
    void remove_from_child_list(Node* node) {
        if (node->left_sibling == node) { // It's the only child
            if (node->parent) node->parent->child = nullptr;
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

    // Adds a node to the root list in O(1) time.
    void add_to_root_list(Node* node) {
        node->parent = nullptr;
        node->marked = false;
        if (min_node_ptr == nullptr) {
            min_node_ptr = node;
            node->left_sibling = node;
            node->right_sibling = node;
        } else {
            Node* last_root = min_node_ptr->left_sibling;
            node->right_sibling = min_node_ptr;
            node->left_sibling = last_root;
            min_node_ptr->left_sibling = node;
            last_root->right_sibling = node;
            if (has_priority(node, min_node_ptr)) {
                min_node_ptr = node;
            }
        }
    }

    // Removes a node from the root list in O(1) time.
    void remove_from_root_list(Node* node) {
        if (node->left_sibling == node) { // It's the only root
            min_node_ptr = nullptr;
        } else {
            node->left_sibling->right_sibling = node->right_sibling;
            node->right_sibling->left_sibling = node->left_sibling;
            if (min_node_ptr == node) {
                min_node_ptr = node->right_sibling;
            }
        }
        node->left_sibling = node;
        node->right_sibling = node;
    }

    // Links two trees of the same rank, making the one with lower priority a child of the other.
    Node* link_trees(Node* root1, Node* root2) {
        if (has_priority(root2, root1)) {
            std::swap(root1, root2);
        }
        add_to_child_list(root1, root2);
        root1->rank++;
        return root1;
    }

    // Consolidates the root list by linking trees of the same rank until no two trees
    // in the root list have the same rank. This is the core of extract-min's cleanup.
    void meld() {
        if (min_node_ptr == nullptr) return;
        std::vector<Node*> rank_array;
        std::vector<Node*> roots_to_process;
        Node* current = min_node_ptr;
        do {
            roots_to_process.push_back(current);
            current = current->right_sibling;
        } while (current != min_node_ptr);

        min_node_ptr = nullptr;

        for (Node* root : roots_to_process) {
            root->left_sibling = root;
            root->right_sibling = root;
            Node* current_root = root;
            while (rank_array.size() <= current_root->rank) {
                rank_array.push_back(nullptr);
            }
            while (rank_array[current_root->rank] != nullptr) {
                Node* other = rank_array[current_root->rank];
                rank_array[current_root->rank] = nullptr;
                current_root = link_trees(current_root, other);
                while (rank_array.size() <= current_root->rank) {
                    rank_array.push_back(nullptr);
                }
            }
            rank_array[current_root->rank] = current_root;
        }

        for (Node* root : rank_array) {
            if (root != nullptr) {
                add_to_root_list(root);
            }
        }
    }

    // Cuts a node from its parent and moves it to the root list.
    // If the parent is marked, a cascading cut is performed on the parent.
    void cut_node(Node* node) {
        if (node->parent == nullptr) return;
        Node* parent = node->parent;
        remove_from_child_list(node);
        parent->rank--;
        add_to_root_list(node);
        if (parent->parent != nullptr) {
            if (parent->marked) {
                cut_node(parent);
            } else {
                parent->marked = true;
            }
        }
    }

    // Finds the new minimum node by scanning the root list.
    // Required after operations that might change the minimum without setting it directly.
    void update_min_node_ptr() {
        if (min_node_ptr == nullptr) return;
        Node* current = min_node_ptr->right_sibling;
        Node* new_min = min_node_ptr;
        while(current != min_node_ptr) {
            if (has_priority(current, new_min)) {
                new_min = current;
            }
            current = current->right_sibling;
        }
        min_node_ptr = new_min;
    }

    // Moves all children of a given node to the root list.
    void merge_children_to_root_list(Node* parent_node) {
        if (parent_node->child == nullptr) return;
        Node* child = parent_node->child;
        do {
            child->parent = nullptr;
            child->marked = false;
            child = child->right_sibling;
        } while (child != parent_node->child);

        if (min_node_ptr == nullptr) {
            min_node_ptr = parent_node->child;
        } else {
            Node* first_child = parent_node->child;
            Node* last_child = first_child->left_sibling;
            Node* last_root = min_node_ptr->left_sibling;
            last_root->right_sibling = first_child;
            first_child->left_sibling = last_root;
            last_child->right_sibling = min_node_ptr;
            min_node_ptr->left_sibling = last_child;
        }
    }

    // Recursively deallocates a tree rooted at a given node.
    void clear_tree(Node* root) {
        if (root == nullptr) return;
        std::vector<Node*> children;
        if (root->child != nullptr) {
            Node* child = root->child;
            do {
                children.push_back(child);
                child = child->right_sibling;
            } while (child != root->child);
        }
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

    // Inserts a new value into the heap. Amortized O(1) time.
//    void insert(const T& value) {
//        if (value_to_node.count(value)) {
//            return; // Value already exists.
//        }
//        Node* new_node = new Node(value, current_timestamp);
//        insertion_order.push_back(current_timestamp);
//        current_timestamp++;
//        add_to_root_list(new_node);
//        value_to_node[value] = new_node;
//    }

    // Leverage Move Semantics to avoid expensive copies and reduce overall overhead
    void insert(T&& value) {
        if (value_to_node.count(value)) {
            return; // Value already exists.
        }
        T value_copy = value;
        Node* new_node = new Node(std::move(value), current_timestamp);
        insertion_order.push_back(current_timestamp);
        current_timestamp++;
        add_to_root_list(new_node);
        value_to_node[std::move(value_copy)] = new_node;
    }

    // Decreases the key of an existing element. Amortized O(1) time.
    // The element is moved to the root list, and its timestamp is updated.
//    void decrease_key(const T& old_value, const T& new_value) {
//        auto it = value_to_node.find(old_value);
//        if (it == value_to_node.end()) {
//            return; // Value doesn't exist.
//        }
//
//        Node* node = it->second;
//        value_to_node.erase(it);
//        node->value = new_value;
//        value_to_node[new_value] = node;
//
//        node->timestamp = current_timestamp;
//        insertion_order.push_back(current_timestamp);
//        current_timestamp++;
//
//        if (node->parent != nullptr && has_priority(node, node->parent)) {
//            cut_node(node);
//        }
//        if (has_priority(node, min_node_ptr)) {
//            min_node_ptr = node;
//        }
//    }

    // Move version
    void decrease_key(T&& old_value, T&& new_value) {
        auto it = value_to_node.find(old_value);
        if (it == value_to_node.end()) {
            return; // Value doesn't exist.
        }

        Node* node = it->second;
        value_to_node.erase(it);

        // Keep a copy of new_value for the map key
        T new_value_copy = new_value;

        node->value = std::move(new_value);
        value_to_node[std::move(new_value_copy)] = node;

        node->timestamp = current_timestamp;
        insertion_order.push_back(current_timestamp);
        current_timestamp++;

        if (node->parent != nullptr && has_priority(node, node->parent)) {
            cut_node(node);
        }
        if (has_priority(node, min_node_ptr)) {
            min_node_ptr = node;
        }
    }

    // Extracts and returns the element with the highest priority (minimum value).
    // Amortized O(log w) time, where w is the size of the working set.
    T extract_min() {
        if (min_node_ptr == nullptr) {
            throw std::runtime_error("Heap is empty");
        }

        Node* min_node = min_node_ptr;
        T min_value = min_node->value;
        // Use RAII wrapper to ensure exception safety
        // unique_ptr(a scope guard), even if an exception is thrown before release(), it won't cause memory leak
        std::unique_ptr<Node> node_guard(min_node);
        value_to_node.erase(min_value);
        remove_from_root_list(min_node);
        if (min_node->child != nullptr) {
            merge_children_to_root_list(min_node);
        }
        delete min_node;
        extract_count++;
        if (min_node_ptr != nullptr) {
            meld();
            update_min_node_ptr();
        }
        node_guard.release();
        return min_value;
    }

    // Returns a const reference to the top element without removing it. O(1) time.
    const T& top() const {
        if (min_node_ptr == nullptr) {
            throw std::runtime_error("Heap is empty");
        }
        return min_node_ptr->value;
    }

    // Checks if the heap is empty.
    bool empty() const {
        return min_node_ptr == nullptr;
    }

    // Returns the number of elements in the heap.
    std::size_t size() const {
        return value_to_node.size();
    }

    // Checks if a value is present in the heap.
    bool contains(const T& value) const {
        return value_to_node.count(value);
    }

    // Deallocates all nodes and resets the heap to an empty state.
    void clear() {
        if (min_node_ptr != nullptr) {
            std::vector<Node*> roots_to_clear;
            Node* current = min_node_ptr;
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

    // (For Debugging) Returns the number of trees in the root list.
    std::size_t get_num_roots() const {
        if (min_node_ptr == nullptr) return 0;
        std::size_t count = 0;
        Node* current = min_node_ptr;
        do {
            count++;
            current = current->right_sibling;
        } while (current != min_node_ptr);
        return count;
    }
};
#endif //TIMESTAMPOPTIMALHEAP_TIMESTAMPOPTIMALHEAP_H
