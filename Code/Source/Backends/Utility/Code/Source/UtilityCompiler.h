#pragma once

#include <UtilityProgram.h>

#include <GOAT/Assets/ProgramAsset.h>
#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Outcome/Outcome.h>

namespace GOAT
{
    //! Turns an authored set of choices into the flat form the backend scores.
    class UtilityCompiler final
    {
    public:
        UtilityCompiler(IAgentSystem& host, const IBlackboardSystem& blackboard);

        AZ::Outcome<UtilityProgram, AZStd::string> Compile(const AZ::Name& name, const AuthoredNode& root) const;

    private:
        //! Reads the properties a program carries about how it picks, rather than what it picks.
        AZ::Outcome<void, AZStd::string> ReadPicking(const AuthoredNode& authored, UtilityProgram& program) const;

        //! Fills in one choice's considerations, how they fold, and the steps it runs.
        AZ::Outcome<void, AZStd::string> EmitChoice(
            const AuthoredNode& authored, UtilityProgram& program, AZ::u16 index) const;

        //! Resolves a `consider` node's blackboard slot.
        AZ::Outcome<BlackboardKey, AZStd::string> ResolveKey(const AuthoredNode& authored) const;

        //! Reads a choice's `combine`, which names a rule or a behaviour and nothing else.
        AZ::Outcome<void, AZStd::string> ResolveCombine(const AuthoredNode& authored, UtilityChoice& choice) const;

        //! Turns one node of a choice's body into the request it runs.
        AZ::Outcome<ActionRequest, AZStd::string> ResolveStep(
            const AuthoredNode& authored, const UtilityChoice& choice, UtilityProgram& program) const;

        IAgentSystem& m_host;
        const IBlackboardSystem& m_blackboard;
    };
} // namespace GOAT
