#pragma once

#include <GOAT/Domain/AgentId.h>
#include <GOAT/GOATDirectorFilterBus.h>
#include <GOAT/GOATTypeIds.h>
#include <GOAT/Interfaces/IDirectorFilter.h>

#include <AzCore/Component/Component.h>

namespace GOAT
{
    //! Narrows a director to the agents standing inside a shape.
    //!
    //! The shape is an ordinary O3DE shape component, so a sphere is the plain radius a director
    //! used to author itself and a box or polygon prism is the zone it never could. Attaching one
    //! of these is the only thing that makes a director less than global.
    class GOATDirectorAreaFilterComponent
        : public AZ::Component
        , public IDirectorFilter
        , public GOATDirectorAreaFilterRequestBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(GOATDirectorAreaFilterComponent);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        // IDirectorFilter
        bool Accepts(AgentId agent, AZ::EntityId entity) const override;

        // GOATDirectorAreaFilterRequestBus
        void SetShapeEntity(AZ::EntityId shape) override;
        AZ::EntityId GetShapeEntity() const override { return m_shape; }

    protected:
        void Activate() override;
        void Deactivate() override;

    private:
        //! Whose shape the area is. This entity unless something aimed it elsewhere.
        AZ::EntityId m_shape;
        //! The director this narrows, or a null handle when it attached to nothing.
        AgentId m_director;
        //! So a missing shape is reported once rather than once per agent per tick.
        mutable bool m_reportedMissingShape = false;
    };
} // namespace GOAT
