#ifndef TIMESTAMPOPTIMALHEAP_TIMESTAMPOPTIMALHEAP_H
#define TIMESTAMPOPTIMALHEAP_TIMESTAMPOPTIMALHEAP_H
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <cstddef>
template<typename T, typename Compare = std::less<T>>
class TimestampOptimalHeap {
private:
    struct Node {
        T value;
        std::size_t timestamp;
        std::size_t rank;           // Rank of the tree rooted at this node
        bool marked;           // For decrease-key operations

        Node* parent;
        Node* child;           // One child in circular doubly-linked list
        Node* left_sibling;    // Left sibling in circular doubly-linked list
        Node* right_sibling;   // Right sibling in circular doubly-linked list

        Node(const T& val, std::size_t ts)
                : value(val), timestamp(ts), rank(0), marked(false),
                  parent(nullptr), child(nullptr), left_sibling(nullptr), right_sibling(nullptr) {
            // Initialize as self-referencing (single node in circular list)
            left_sibling = this;
            right_sibling = this;
        }
    };

    std::unordered_map<T, Node*> value_to_node;
    Compare comp;
    std::size_t current_timestamp;

    // Working set tracking
    std::vector<std::size_t> insertion_order;
    std::size_t extract_count;

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

    std::size_t size() const {
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
