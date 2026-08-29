#pragma once


#include <GOAT/Assets/ProgramAsset.h>
#include <GOAT_BehaviorTree/DecisionProgram.h>
#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! Turns an authored tree into the flat form the walker executes.
    //! Both Lua and a future graph editor produce the asset this consumes, so the
    //! runtime never learns which one an agent's tree came from.
    class TreeCompiler final
    {
    public:
        TreeCompiler(IAgentSystem& host, const IBlackboardSystem& blackboard);

        //! Compiles an authored tree. On failure the message names the offending node.
        AZ::Outcome<DecisionProgram, AZStd::string> Compile(const AZ::Name& name, const AuthoredNode& root) const;

    private:
        //! Emits one node and its subtree, returning the index it was written to.
        //! m_inlining names the trees currently being expanded, which is how a cycle is caught.
        AZ::Outcome<NodeIndex, AZStd::string> Emit(
            const AuthoredNode& authored,
            NodeIndex parent,
            AZ::u32 depth,
            DecisionProgram& program,
            AZStd::vector<AZ::Name>& inlining) const;

        //! Expands a subtree reference in place of the referencing node.
        AZ::Outcome<NodeIndex, AZStd::string> Inline(
            const AuthoredNode& authored,
            NodeIndex parent,
            AZ::u32 depth,
            DecisionProgram& program,
            AZStd::vector<AZ::Name>& inlining) const;

        //! Checks an authored node's properties against what its type accepts.
        //! Checks a parallel's background branch and registers what it observes.
        AZ::Outcome<void, AZStd::string> RegisterParallel(NodeIndex index, DecisionProgram& program) const;

        AZ::Outcome<void, AZStd::string> Validate(
            const AuthoredNode& authored, const NodeTypeDescriptor& descriptor) const;

        IAgentSystem& m_host;
        const IBlackboardSystem& m_blackboard;
    };
} // namespace GOAT
