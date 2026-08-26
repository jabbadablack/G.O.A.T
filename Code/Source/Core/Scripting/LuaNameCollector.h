#pragma once

#include <GOAT/GOATTypeIds.h>

#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! Collects a list of names Lua hands over.
    //! Used instead of reading a Lua table from C++, so name marshalling stays in the
    //! reflection layer like every other call in this gem.
    class LuaNameCollector final
    {
    public:
        AZ_TYPE_INFO(LuaNameCollector, LuaNameCollectorTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Discards anything previously collected.
        void Clear();

        //! Appends one name.
        void Add(AZStd::string name);

        const AZStd::vector<AZ::Name>& GetNames() const { return m_names; }

    private:
        AZStd::vector<AZ::Name> m_names;
    };
} // namespace GOAT
