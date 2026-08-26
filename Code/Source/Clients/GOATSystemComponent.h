#pragma once

#include <Core/Application/BlackboardSystem.h>

#include <GOAT/GOATBus.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace AzFramework
{
    class GenericAssetHandlerBase;
}

namespace GOAT
{
    //! Owns every GOAT service and registers them for the lifetime of the gem.
    //! The editor system component derives from this, so registering here covers both modules.
    class GOATSystemComponent
        : public AZ::Component
        , protected GOATRequestBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(GOATSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        GOATSystemComponent();
        ~GOATSystemComponent();

    protected:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

    private:
        //! Registers the asset handlers this gem owns. Safe to call when another module already did.
        void RegisterAssetHandlers();
        void UnregisterAssetHandlers();

        AZStd::unique_ptr<BlackboardSystem> m_blackboardSystem;
        AZStd::vector<AZStd::unique_ptr<AZ::Data::AssetHandler>> m_assetHandlers;
    };
} // namespace GOAT
