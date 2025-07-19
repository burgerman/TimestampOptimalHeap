#ifndef TIMESTAMPOPTIMALHEAP_GRAPH_H
#define TIMESTAMPOPTIMALHEAP_GRAPH_H
#include <vector>
#include <cstddef>
#include <unordered_map>
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

    std::size_t num_nodes() const {
        return adjacency_list.size();
    }
};
#endif //TIMESTAMPOPTIMALHEAP_GRAPH_H
