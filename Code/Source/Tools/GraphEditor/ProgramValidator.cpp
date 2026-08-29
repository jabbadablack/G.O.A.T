#include <Tools/GraphEditor/ProgramValidator.h>

#include <GOAT/GOATBackendBus.h>
#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/algorithm.h>

namespace GOAT::GraphEditor
{
    namespace
    {
        constexpr const char* DefaultBackend = "tree";

        bool ArityIsLegal(NodeKind kind, size_t children)
        {
            switch (kind)
            {
            case NodeKind::Composite: return children > 0;
            case NodeKind::Decorator: return children == 1;
            default:                  return children == 0;
            }
        }

        //! Checks one node against what its word declares, which is where a typo, a missing
        //! required property or a wrong child count is caught, per node.
        bool CheckNode(const AuthoredNode& authored, const AZ::Name& backend,
            AZStd::vector<size_t>& path, ValidationResult& result)
        {
            IAgentSystem* agents = AgentSystemInterface::Get();
            const NodeTypeDescriptor* descriptor =
                agents != nullptr ? agents->FindNodeType(AZ::Name(authored.m_type)) : nullptr;

            auto fail = [&result, &path](AZStd::string message)
            {
                result.m_valid = false;
                result.m_error = AZStd::move(message);
                result.m_path = path;
                return false;
            };

            if (descriptor == nullptr)
            {
                return fail(AZStd::string::format("'%s' is not a word any backend registered",
                    authored.m_type.c_str()));
            }

            if (!descriptor->m_backend.IsEmpty() && descriptor->m_backend != backend)
            {
                return fail(AZStd::string::format("'%s' belongs to the '%s' backend, not to '%s'",
                    authored.m_type.c_str(), descriptor->m_backend.GetCStr(), backend.GetCStr()));
            }

            for (const AuthoredProperty& property : authored.m_properties)
            {
                const AZ::Name name(property.m_name);
                const bool accepted = AZStd::any_of(descriptor->m_parameters.begin(),
                    descriptor->m_parameters.end(),
                    [&name](const NodeParameter& parameter) { return parameter.m_name == name; });
                if (!accepted)
                {
                    return fail(AZStd::string::format("'%s' has no property '%s'",
                        authored.m_type.c_str(), property.m_name.c_str()));
                }
            }

            for (const NodeParameter& parameter : descriptor->m_parameters)
            {
                if (!parameter.m_required)
                {
                    continue;
                }
                const bool given = AZStd::any_of(authored.m_properties.begin(), authored.m_properties.end(),
                    [&parameter](const AuthoredProperty& property)
                    { return AZ::Name(property.m_name) == parameter.m_name; });
                if (!given)
                {
                    return fail(AZStd::string::format("'%s' requires property '%s'",
                        authored.m_type.c_str(), parameter.m_name.GetCStr()));
                }
            }

            if (!ArityIsLegal(descriptor->m_kind, authored.m_children.size()))
            {
                return fail(AZStd::string::format("'%s' cannot have %zu children",
                    authored.m_type.c_str(), authored.m_children.size()));
            }

            for (size_t i = 0; i < authored.m_services.size(); ++i)
            {
                path.push_back(i);
                const bool ok = CheckNode(authored.m_services[i], backend, path, result);
                path.pop_back();
                if (!ok)
                {
                    return false;
                }
            }

            // Children continue the index space the services started, so a path step names one
            // node rather than one of two.
            for (size_t i = 0; i < authored.m_children.size(); ++i)
            {
                path.push_back(authored.m_services.size() + i);
                const bool ok = CheckNode(authored.m_children[i], backend, path, result);
                path.pop_back();
                if (!ok)
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    AZ::Name FindOwningBackend(const AuthoredNode& root)
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            return {};
        }

        const NodeTypeDescriptor* descriptor = agents->FindNodeType(AZ::Name(root.m_type));
        if (descriptor == nullptr)
        {
            return {};
        }
        // A core word roots a behaviour tree, which is what an agent runs unless it says otherwise.
        return descriptor->m_backend.IsEmpty() ? AZ::Name(DefaultBackend) : descriptor->m_backend;
    }

    ValidationResult Validate(const AuthoredNode& root, const AZ::Name& programName)
    {
        ValidationResult result;

        const AZ::Name backendName = FindOwningBackend(root);
        if (backendName.IsEmpty())
        {
            result.m_valid = false;
            result.m_error = AZStd::string::format("'%s' is not a word any backend registered",
                root.m_type.c_str());
            return result;
        }

        // Per node first, so a fault can be pointed at the node that carries it.
        AZStd::vector<size_t> path;
        if (!CheckNode(root, backendName, path, result))
        {
            return result;
        }

        // Then the whole program through the real compiler, which is the only thing that knows
        // about cycles, depth limits and whether a named variable was ever declared.
        IDecisionBackend* backend = nullptr;
        GOATBackendRequestBus::BroadcastResult(
            backend, &GOATBackendRequests::FindDecisionBackend, backendName);
        if (backend == nullptr)
        {
            result.m_valid = false;
            result.m_error = AZStd::string::format("The '%s' backend is not installed",
                backendName.GetCStr());
            return result;
        }

        if (auto compiled = backend->Compile(programName, root); !compiled.IsSuccess())
        {
            result.m_valid = false;
            result.m_error = compiled.TakeError();
        }
        return result;
    }
} // namespace GOAT::GraphEditor
