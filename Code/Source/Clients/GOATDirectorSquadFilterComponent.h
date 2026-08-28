#pragma once

#include <GOAT/Domain/AgentId.h>
#include <GOAT/GOATTypeIds.h>
#include <GOAT/Interfaces/IDirectorFilter.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! Narrows a director to the agents in named squads or carrying named tags.
    //!
    //! The two lists union: an agent is governed if its squad is listed or any of its tags is.
    //! Tags are the engine's own, so an author marks an agent with the stock Tag component and
    //! nothing in GOAT has to learn what a tag is.
    class GOATDirectorSquadFilterComponent
        : public AZ::Component
        , public IDirectorFilter
    {
    public:
        AZ_COMPONENT_DECL(GOATDirectorSquadFilterComponent);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        // IDirectorFilter
        bool Accepts(AgentId agent, AZ::EntityId entity) const override;

    protected:
        void Activate() override;
        void Deactivate() override;

    private:
        //! Authored as strings because that is what the property editor can show; converted to
        //! the interned form once, when this component activates.
        AZStd::vector<AZStd::string> m_squads;
        AZStd::vector<AZStd::string> m_tags;

        AZStd::vector<AZ::Name> m_squadNames;
        AZStd::vector<AZ::Crc32> m_tagIds;

        //! The director this narrows, or a null handle when it attached to nothing.
        AgentId m_director;
    };
} // namespace GOAT
