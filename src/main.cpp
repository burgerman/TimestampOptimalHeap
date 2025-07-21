#include <iostream>
#include <string>
#include <chrono>
#include <cstddef>
#include "Dijkstra.h"
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
            for (std::size_t i = 0; i < path.size(); ++i) {
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
        for (std::size_t i = 0; i < path.size(); ++i) {
            std::cout << path[i];
            if (i < path.size() - 1) std::cout << " -> ";
        }
        std::cout << "\nDistance: " << result_single.get_distance_to(5) << "\n";
    } else {
        std::cout << "No path found from 1 to 5\n";
    }
}

void func_perform_test() {
    TimestampOptimalHeap<int> heap(0.7f, 200'000);

    std::cout << "=== Testing O(1) Operations ===" << std::endl;

    // Test O(1) insert
    auto start = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < 100000; ++i) {
        heap.insert(rand() % 500000);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "100,000 inserts in " << duration.count() << " microseconds" << std::endl;

    // Test O(1) top operation
    start = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < 10000; ++i) {
        volatile int min_val = heap.top();  // volatile to prevent optimization
        (void)min_val;  // suppress unused variable warning
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "10,000 top() calls in " << duration.count() << " microseconds" << std::endl;

    // Test decrease-key performance
    std::vector<int> values;
    for (std::size_t i = 0; i < 1000; ++i) {
        std::size_t val = rand() % 100000;
        heap.insert(val);
        values.push_back(val);
    }

    start = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < 500; ++i) {
        std::size_t old_val = values[i];
        std::size_t new_val = (old_val - 1000)>0?(old_val - 1000) : (old_val/2);  // Make it smaller
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
    for (std::size_t i = 0; i < 20; ++i) {
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

int main() {
    func_perform_test();
    std::cout << "\n====================================================" << std::endl;
    example_dijkstra();
    return 0;
}