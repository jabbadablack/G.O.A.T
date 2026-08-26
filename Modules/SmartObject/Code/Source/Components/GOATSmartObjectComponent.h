#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT_SmartObject
{
    //! Offers this entity to GOAT agents as something they can come and use.
    //!
    //! The entity does not have to know anything about AI: it declares what it is for, where to
    //! stand, and how many agents fit. Everything else is the agent's tree.
    class GOATSmartObjectComponent final
        : public AZ::Component
    {
    public:
        AZ_COMPONENT_DECL(GOATSmartObjectComponent);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

    protected:
        void Activate() override;
        void Deactivate() override;

    private:
        //! What an agent asks for, as in "sit" or "repair". One entity may offer several.
        AZStd::vector<AZStd::string> m_uses;

        //! Where the agent should stand, relative to this entity.
        AZ::Vector3 m_anchorOffset = AZ::Vector3::CreateZero();

        //! How many agents may use it at once.
        AZ::u32 m_capacity = 1;
    };
} // namespace GOAT_SmartObject
