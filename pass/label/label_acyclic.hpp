// This file is distributed under the BSD 3-Clause License. See LICENSE for details.

#pragma once

#include <vector>

#include "lgedgeiter.hpp"
#include "lgraph.hpp"
#include "lgraphbase.hpp"
#include "lnast.hpp"
#include "pass.hpp"

class Label_acyclic {
private:
  const bool verbose;
  const bool hier;
  const bool merge_en;
  uint8_t    cutoff;  // currently not being used
  uint8_t    part_id;

  using NodeVector = std::vector<Node::Compact>;
  using NodeSet    = absl::flat_hash_set<Node::Compact>;

  NodeVector node_preds;  // predecessors of a node
  NodeSet    roots;       // potential roots of partitions

  absl::flat_hash_map<Node::Compact, int> node2id;   // <Node, Partition ID>
  absl::flat_hash_map<int, NodeSet>       id2nodes;  // <Partition ID, Nodes in Partition>
  absl::flat_hash_map<int, NodeSet>       id2inc;    // <Partition ID, Incoming Neighbors>
  absl::flat_hash_map<int, NodeSet>       id2out;    // <Partition ID, Outgoing Neighbors>

  bool set_cmp(NodeSet a, NodeSet b) const;

  void gather_roots(Lgraph *g);
  void grow_partitions(Lgraph *g);
  void merge_partitions();

public:
  void label(Lgraph *g);

  Label_acyclic(bool _verbose, bool _hier, uint8_t _cutoff, bool _merge_en);

  void dump() const;
};
