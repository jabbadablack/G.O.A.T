#pragma once

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! Registers the gem's asset handlers inside the Asset Processor's builder processes.
    //!
    //! Those processes build an entity only from components tagged AssetBuilder, so the
    //! ordinary system component never activates there. Without this, no handler is
    //! registered, the generic asset builder finds nothing to claim, and a .bbx source is
    //! silently ignored. It deliberately registers handlers and nothing else: a builder
    //! process has no use for agents, scheduling or Lua.
    class GOATBuilderComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT_DECL(GOATBuilderComponent);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        GOATBuilderComponent();
        ~GOATBuilderComponent();

        AZ_DISABLE_COPY_MOVE(GOATBuilderComponent);

    protected:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

    private:
        AZStd::vector<AZStd::unique_ptr<AZ::Data::AssetHandler>> m_assetHandlers;
    };
} // namespace GOAT
