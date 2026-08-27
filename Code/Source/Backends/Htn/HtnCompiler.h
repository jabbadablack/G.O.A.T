#pragma once

#include <Backends/Htn/HtnDomain.h>

#include <GOAT/Assets/BehaviorTreeAsset.h>
#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! Turns an authored task network into the flat form the planner decomposes.
    class HtnCompiler final
    {
    public:
        HtnCompiler(IAgentSystem& host, const IBlackboardSystem& blackboard);

        AZ::Outcome<HtnDomain, AZStd::string> Compile(const AZ::Name& name, const AuthoredNode& root) const;

    private:
        //! Fills in one compound task's methods.
        AZ::Outcome<void, AZStd::string> EmitTask(const AuthoredNode& authored, HtnDomain& domain, AZ::u16 index) const;

        //! Fills in one primitive's conditions, operator and effects.
        AZ::Outcome<void, AZStd::string> EmitPrimitive(
            const AuthoredNode& authored, HtnDomain& domain, AZ::u16 index) const;

        //! Resolves a `condition` or `effect` node's blackboard slot.
        AZ::Outcome<BlackboardKey, AZStd::string> ResolveKey(const AuthoredNode& authored) const;

        //! Turns an operator leaf into the request it runs.
        AZ::Outcome<ActionRequest, AZStd::string> ResolveOperator(const AuthoredNode& authored) const;

        IAgentSystem& m_host;
        const IBlackboardSystem& m_blackboard;
    };
} // namespace GOAT
