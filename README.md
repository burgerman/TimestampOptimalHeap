# ⏱️ Timestamp-Optimal Heap

A C++ implementation of a **Timestamp-Optimal Heap** — a custom priority queue designed to replace the traditional Fibonacci heap,
exploiting **temporal locality** in practical settings of finding the shortest path in Dijkstras's algo.
The main goal is to **accelerate algorithms like Dijkstra’s** by avoiding **worst case**.

Inspired by the research breakthroughs in:
- _Universal Optimality of Dijkstra via Beyond-Worst-Case Heaps_
- _Simple Universally Optimal Dijkstra_

## 📌 References
- Haeupler, B., Hladík, R., Rozhoň, V., Tarjan, R. E., & Tetĕk, J. (2024, October). _Universal Optimality of Dijkstra via Beyond-Worst-Case Heaps_. In *2024 IEEE 65th Annual Symposium on Foundations of Computer Science (FOCS)* (pp. 2099–2130). IEEE. [https://doi.org/10.1109/FOCS57990.2024.00154](https://doi.org/10.1109/FOCS57990.2024.00154)

- van der Hoog, I., Rotenberg, E., & Rutschmann, D. (2025). _Simple Universally Optimal Dijkstra_. *arXiv preprint arXiv:2504.17327*. [https://arxiv.org/abs/2504.17327](https://arxiv.org/abs/2504.17327)


---

## 📜 Background

Traditional priority queues (binary-heap data structure) provide `O(log n)` amortized time for insert and delete operations. 
While this is efficient in small problems, it is believed a bottleneck for real-world graph searching problems, 
where node access patterns often show **temporal locality**.

These papers introduce a new heap structure that utilizes the **working-set property**,
in order to limit the overhead of getting the min value from a min heap after removing it from the heap.
- The key idea is that the cost of deleting the min node is logarithmic in the number of nodes inserted after it, but before it is deleted, instead of logarithmic in the size of the heap when the node is deleted. This makes use of locality in the heap operations to achieve universal optimality.
- Unlike Multi-Level Dijkstra with Contraction Hierarchies in Open Source Routing Machine (OSRM), working based on the cached order, this solution doesn't require preprocessing.
## Goals:
- 🧮 **Insert:** Amortized `O(1)` time
- 🔁 **Decrease-Key:** Amortized `O(1)` time
- 📤 **Extract-Min:** Amortized `O(log w)` time
  (_where_ `w` _is the working-set size – the number of operations since the item was inserted_)

---

## 🛠 Core Concepts & Implementation Strategy

This is not a standard binary or Fibonacci heap. The design blends a couple of advanced strategies to achieve optimal performance:

### 🌲 1. Forest of Heap-Ordered Trees
- Structure resembles a **Fibonacci Heap**: a list of tree roots ordered by heap property.
- **Lazy insertions**: elements are simply added to the root list without restructuring.

### ⏱️ 2. Working-Set Property via Timestamps
- Every node gets a **timestamp** upon insertion or key update.
- During `extract-min`, only **"hot" nodes** (recently accessed) are processed.
- This ensures operations remain bounded by `log(w)`, not `log(n)`.

### 🔧 3. Key Operations

#### `insert()` — **Amortized O(1)**
- Adds a new, single-node tree to the root list.

#### `decrease_key()` — **Amortized O(1)**
- Cuts the node from its parent and promotes it to the root list.
- Efficient thanks to:
    - **Circular, doubly-linked child lists** for `O(1)` removals.
    - **Cascading cuts** to maintain balanced trees.

#### `extract_min()` — **Amortized O(log w)** ✨
- Cached pointer gives `O(1)` access to the min node.
- Children of the min node are promoted to the root list.
- **Consolidation** only affects **"hot" roots** (with timestamps ≥ extracted node).
    - This limits work done to size `w`, not `n`.

---

## 🚀 Features

- ✅ **Generic & Templated:**  
  `template<typename T, typename Compare>` supports any data type and custom comparison logic.

- 🧩 **Efficient Node Lookups:**  
  Uses `std::unordered_map` for `O(1)` average-time access during `decrease-key`.

- 🔍 **O(1) top() Operation:**  
  Maintains a cached pointer to the current min node.

- 🧼 **Robust Memory Management:**
    - Properly follows the **Rule of Three/Five**.
    - Disables copy/assignment to avoid accidental deep copies.
    - Periodically **prunes the insertion log** to prevent memory bloat.

- ⚖️ **Deterministic Tie-Breaking:**
    - Timestamps act as a secondary criterion for comparing equal-priority elements.
    - Guarantees **stable and predictable ordering**.

---
