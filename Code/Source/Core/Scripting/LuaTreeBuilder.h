#pragma once

#include <GOAT/Assets/ProgramAsset.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! Assembles an authored tree from the flat calls the Lua vocabulary makes.
    //! Lua pushes into this rather than C++ reading the Lua stack, so all marshalling
    //! happens in the reflection layer where it is type checked.
    class LuaTreeBuilder final
    {
    public:
        AZ_TYPE_INFO(LuaTreeBuilder, LuaTreeBuilderTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Starts a new tree, discarding anything previously built.
        void BeginTree(AZStd::string name);

        //! Appends one node in pre-order, declaring how many services and children follow it.
        void AddNode(AZStd::string type, int childCount, int serviceCount);

        //! Sets a property on the most recently added node.
        void SetBoolProperty(AZStd::string key, bool value);
        void SetNumberProperty(AZStd::string key, double value);
        void SetStringProperty(AZStd::string key, AZStd::string value);

        //! Rebuilds the nesting from the pre-order counts.
        void EndTree();

        //! True when a tree was emitted and rebuilt without error.
        bool IsComplete() const { return m_complete; }

        //! The assembled root, valid once the emission completed.
        //! The builder deliberately does not own an asset: AZ::Data::AssetData is refcounted
        //! and not copyable, so the caller wraps this in one.
        const AuthoredNode& GetRoot() const { return m_root; }

        //! Name the emitted tree declared.
        const AZStd::string& GetTreeName() const { return m_name; }

        //! Why the last emission failed, when it did.
        const AZStd::string& GetError() const { return m_error; }

    private:
        //! One node as emitted, before nesting is rebuilt.
        struct Record final
        {
            AZStd::string m_type;
            int m_childCount = 0;
            int m_serviceCount = 0;
            AZStd::vector<AuthoredProperty> m_properties;
        };

        //! Consumes the record at an index and its descendants, returning the next index.
        size_t Build(size_t index, AuthoredNode& out);

        AZStd::vector<Record> m_records;
        AuthoredNode m_root;
        AZStd::string m_name;
        AZStd::string m_error;
        bool m_complete = false;
    };
} // namespace GOAT
