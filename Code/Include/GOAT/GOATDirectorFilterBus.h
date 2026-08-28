#pragma once

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Component/EntityId.h>

namespace GOAT
{
    //! Points a director's area filter at a shape.
    //!
    //! Only here, not on the component, because the common case is the shape beside the filter and
    //! a required ShapeService already guarantees that one. Aiming at another entity is what a
    //! zone outliving a roaming director needs, and that is set up by whatever spawned them.
    class GOATDirectorAreaFilterRequests : public AZ::ComponentBus
    {
    public:
        //! The entity whose shape is the area. An invalid id means the filter's own entity.
        virtual void SetShapeEntity(AZ::EntityId shape) = 0;
        virtual AZ::EntityId GetShapeEntity() const = 0;
    };

    using GOATDirectorAreaFilterRequestBus = AZ::EBus<GOATDirectorAreaFilterRequests>;
} // namespace GOAT
