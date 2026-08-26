#pragma once

#include <GOAT/Domain/BlackboardTypes.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/any.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! One variable declared by a .bbx asset.
    //! Only the default matching the chosen type is shown in the editor and used at load.
    class BlackboardVariable final
    {
    public:
        AZ_TYPE_INFO(BlackboardVariable, BlackboardVariableTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! The declared default as an any, or an empty any when this type has no editable default.
        AZStd::any GetDefault() const;

        //! Name this variable is referenced by. Shared across every loaded .bbx asset.
        AZStd::string m_name;
        //! Which lifetime the variable belongs to.
        BlackboardScope m_scope = BlackboardScope::Agent;
        //! What kind of value the variable holds.
        BlackboardType m_type = BlackboardType::Bool;

        bool m_boolDefault = false;
        AZ::s64 m_intDefault = 0;
        float m_floatDefault = 0.0f;
        AZ::Vector3 m_vector3Default = AZ::Vector3::CreateZero();
        AZ::EntityId m_entityIdDefault;
        AZStd::string m_nameDefault;

    private:
        //! Shows a default field only when it matches the chosen type.
        AZ::Crc32 GetBoolVisibility() const;
        AZ::Crc32 GetIntVisibility() const;
        AZ::Crc32 GetFloatVisibility() const;
        AZ::Crc32 GetVector3Visibility() const;
        AZ::Crc32 GetEntityIdVisibility() const;
        AZ::Crc32 GetNameVisibility() const;
    };

    //! A set of blackboard variable declarations, authored in the Asset Editor.
    //! Every loaded asset merges into one namespace, so names must be unique across all of them.
    class BlackboardAsset final
        : public AZ::Data::AssetData
    {
    public:
        AZ_RTTI(BlackboardAsset, BlackboardAssetTypeId, AZ::Data::AssetData);
        AZ_CLASS_ALLOCATOR(BlackboardAsset, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        //! Source extension the Asset Processor watches for.
        static constexpr const char* FileExtension = "bbx";
        //! Group this asset is filed under in the Asset Editor.
        static constexpr const char* AssetGroup = "GOAT";
        //! Name shown in the Asset Editor's new asset list.
        static constexpr const char* DisplayName = "Blackboard";

        AZStd::vector<BlackboardVariable> m_variables;
    };
} // namespace GOAT
