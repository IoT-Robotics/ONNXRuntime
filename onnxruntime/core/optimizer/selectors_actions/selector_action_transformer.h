// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <functional>
#if !defined(ORT_MINIMAL_BUILD)
#include <optional>
#endif  // !defined(ORT_MINIMAL_BUILD)
#include <variant>

#include "core/framework/kernel_registry_manager.h"
#include "core/optimizer/graph_transformer.h"
#include "core/optimizer/selectors_actions/actions.h"
#include "core/optimizer/selectors_actions/selector_action_transformer_apply_contexts.h"

namespace onnxruntime {

class Graph;
class GraphViewer;
class Node;

#if !defined(ORT_MINIMAL_BUILD)

// Base class for a selector which checks for a match and returns the set of nodes involved.
struct NodeSelector {
  // Select one or more nodes for an Action to process if the constraints are satisfied,
  // otherwise returns std::nullopt
  virtual std::optional<NodesToOptimizeIndices> Select(const GraphViewer& graph_viewer, const Node& node) const = 0;

  virtual ~NodeSelector() = default;

 protected:
  NodeSelector() = default;
};

struct SelectorAndAction {
  using OpVersionsMap = std::unordered_map<std::string, std::vector<ONNX_NAMESPACE::OperatorSetVersion>>;

  // ctor so we can use make_unique to construct this class
  SelectorAndAction(const std::string& name_in,
                    const OpVersionsMap& ops_and_versions_in,
                    std::unique_ptr<NodeSelector> selector_in,
                    std::unique_ptr<Action> action_in)
      : name{name_in},
        ops_and_versions{ops_and_versions_in},
        selector{std::move(selector_in)},
        action{std::move(action_in)} {}

  const std::string name;
  OpVersionsMap ops_and_versions;
  std::unique_ptr<NodeSelector> selector;
  std::unique_ptr<Action> action;

  // can't copy/assign our unique_ptr members
  ORT_DISALLOW_COPY_AND_ASSIGNMENT(SelectorAndAction);
};

#endif  // !defined(ORT_MINIMAL_BUILD)

// class to manage a set of selector and associated actions
class SelectorsAndActions {
 public:
  SelectorsAndActions() = default;
  SelectorsAndActions(SelectorsAndActions&&) = default;

  ORT_DISALLOW_COPY_AND_ASSIGNMENT(SelectorsAndActions);

#if !defined(ORT_MINIMAL_BUILD)

  // register a selector and action for the specified ops.
  // the name used in the registration is for matching the action when replaying the optimizations in a minimal build.
  // as it's stored in the ORT format model a shorter name is better. the name is scoped to this SelectorsAndActions
  // instance (which is scoped to a single SelectorActionTransformer instance).
  void RegisterSelectorAndAction(const std::string& name,
                                 const SelectorAndAction::OpVersionsMap& ops_and_versions,
                                 std::unique_ptr<NodeSelector> selector,
                                 std::unique_ptr<Action> action);

  const std::unordered_map<std::string, std::unique_ptr<SelectorAndAction>>& SelectorsAndActionsMap() const {
    return selectors_and_actions_map_;
  }

#else  // !defined(ORT_MINIMAL_BUILD)

  // register an action
  void RegisterAction(const std::string& name, std::unique_ptr<Action> action);

#endif  // !defined(ORT_MINIMAL_BUILD)

  // return registered Action or nullptr if not found
  const Action* LookUpAction(const std::string& name) const;

 private:
#if !defined(ORT_MINIMAL_BUILD)
  std::unordered_map<std::string, std::unique_ptr<SelectorAndAction>> selectors_and_actions_map_;
#else   // !defined(ORT_MINIMAL_BUILD)
  std::unordered_map<std::string, std::unique_ptr<Action>> actions_map_;
#endif  // !defined(ORT_MINIMAL_BUILD)
};

/**
Class that implements graph transformation via a set of Selector+Action pairs.
This setup allows optimizations to be captured and applied at runtime in a minimal build.
*/
class SelectorActionTransformer : public GraphTransformer {
 protected:
  SelectorActionTransformer(const std::string& name, SelectorsAndActions&& selectors_and_actions,
                            const SatApplyContextVariant& apply_context);

  // can't copy/assign selectors_and_actions_
  ORT_DISALLOW_COPY_AND_ASSIGNMENT(SelectorActionTransformer);

 private:
  Status ApplyImpl(Graph& graph, bool& modified, int graph_level, const logging::Logger& logger) const override;

#if !defined(ORT_MINIMAL_BUILD)

  Status ApplyDirect(Graph& graph, bool& modified, int graph_level, const logging::Logger& logger,
                     const SatRuntimeOptimizationSaveContext* save_context) const;

  // check if the node matches any of the registered operators.
  // if it does, run the Selector.
  // if that selects nodes, run the Action.
  //
  // Some part of the MatchAndProcess use a GraphViewer of the given graph,
  // we choose to supply both the graph and the graph_viewer to avoid expensive
  // and repeatedly construction of the graph_viewer.
  // NOTE, the graph must be the same as the graph_viewer's underlying graph
  Status MatchAndProcess(Graph& graph, const GraphViewer& graph_viewer, Node& node,
                         bool& modified, const logging::Logger& logger,
                         const SatRuntimeOptimizationSaveContext* save_context) const;

#endif  // !defined(ORT_MINIMAL_BUILD)

#if defined(ORT_ENABLE_ORT_FORMAT_RUNTIME_GRAPH_OPTIMIZATION)
  // apply any saved optimizations
  Status ApplyFromRuntimeOptimizations(Graph& graph, bool& modified, int graph_level,
                                       const logging::Logger& logger) const;
#endif  // defined(ORT_ENABLE_ORT_FORMAT_RUNTIME_GRAPH_OPTIMIZATION)

  SelectorsAndActions selectors_and_actions_;

  SatApplyContextVariant apply_context_;

#if !defined(ORT_MINIMAL_BUILD)
  std::unordered_map<std::string, const SelectorAndAction*> op_type_to_selector_and_action_;
#endif  // !defined(ORT_MINIMAL_BUILD)
};

}  // namespace onnxruntime
