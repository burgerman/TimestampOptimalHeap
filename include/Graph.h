#ifndef TIMESTAMPOPTIMALHEAP_GRAPH_H
#define TIMESTAMPOPTIMALHEAP_GRAPH_H
#include <vector>
#include <cstddef>
#include <unordered_map>
template<typename NodeId = int, typename Weight = double>
// Represents a generic directed graph using an adjacency list.
class Graph {
public:
    // Represents a directed edge with a destination node and a weight.
    struct Edge {
        NodeId to;
        Weight weight;

        Edge(NodeId dest, Weight w) : to(dest), weight(w) {}
    };

private:
    // Maps each node to a vector of its outgoing edges.
    std::unordered_map<NodeId, std::vector<Edge>> adjacency_list;

public:
    // Adds a directed edge from one node to another.
    void add_edge(NodeId from, NodeId to, Weight weight) {
        adjacency_list[from].emplace_back(to, weight);
    }

    // Adds an edge in both directions, effectively creating an undirected edge.
    void add_bidirectional_edge(NodeId from, NodeId to, Weight weight) {
        add_edge(from, to, weight);
        add_edge(to, from, weight);
    }

    // Retrieves all outgoing edges for a given node.
    // Returns an empty vector if the node has no neighbors or does not exist.
    const std::vector<Edge>& get_neighbors(NodeId node) const {
        static const std::vector<Edge> empty_vector;
        auto it = adjacency_list.find(node);
        return (it != adjacency_list.end()) ? it->second : empty_vector;
    }

    // Returns a list of all unique nodes present in the graph.
    std::vector<NodeId> get_all_nodes() const {
        std::vector<NodeId> nodes;
        for (const auto& pair : adjacency_list) {
            nodes.push_back(pair.first);
        }
        return nodes;
    }

    // Checks if a node exists in the graph.
    bool has_node(NodeId node) const {
        return adjacency_list.find(node) != adjacency_list.end();
    }

    // Returns the total number of nodes (vertices) in the graph.
    std::size_t num_nodes() const {
        return adjacency_list.size();
    }
};
#endif //TIMESTAMPOPTIMALHEAP_GRAPH_H
