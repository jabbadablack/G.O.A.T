#include <Tools/GraphEditor/GraphContext.h>
#include <Tools/GraphEditor/Core.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/string/string.h>

namespace GOAT::GraphEditor
{
    namespace
    {
        //! A blackboard value is authored as a literal or as the name of a variable, and a
        //! variable's name is a string either way, so every type a slot can hold is one of
        //! these four as far as the property editor is concerned.
        template<typename T>
        AZStd::shared_ptr<GraphModel::DataType> Type(
            ProgramDataType type, const T& defaultValue, const char* displayName, const char* cppName)
        {
            return AZStd::make_shared<GraphModel::DataType>(
                static_cast<GraphModel::DataType::Enum>(type), AZ::AzTypeInfo<T>::Uuid(),
                AZStd::any(defaultValue), displayName, cppName);
        }
    } // namespace

    ProgramDataType ToDataType(BlackboardType type)
    {
        switch (type)
        {
        case BlackboardType::Bool:         return ProgramDataType::Bool;
        case BlackboardType::Int:          return ProgramDataType::Int;
        case BlackboardType::Float:        return ProgramDataType::Float;
        case BlackboardType::Vector3:      return ProgramDataType::Vector3;
        case BlackboardType::EntityId:     return ProgramDataType::EntityId;
        case BlackboardType::Name:         return ProgramDataType::Name;
        case BlackboardType::Quaternion:   return ProgramDataType::Quaternion;
        case BlackboardType::Transform:    return ProgramDataType::Transform;
        case BlackboardType::EntityIdList: return ProgramDataType::EntityIdList;
        default:                           return ProgramDataType::Name;
        }
    }

    AZStd::shared_ptr<GraphContext> GraphContext::s_instance;

    void GraphContext::SetInstance(AZStd::shared_ptr<GraphContext> context)
    {
        s_instance = AZStd::move(context);
    }

    AZStd::shared_ptr<GraphContext> GraphContext::GetInstance()
    {
        return s_instance;
    }

    GraphContext::GraphContext()
        : GraphModel::GraphContext(SystemName, ModuleFileExtension, {})
    {
        // An execution edge carries nothing; it only says which node runs under which.
        m_dataTypes.push_back(Type(ProgramDataType::Execution, AZStd::string{}, "Execution", "void"));

        m_dataTypes.push_back(Type(ProgramDataType::Bool, false, "Bool", "bool"));
        m_dataTypes.push_back(Type(ProgramDataType::Int, 0, "Int", "int"));
        m_dataTypes.push_back(Type(ProgramDataType::Float, 0.0f, "Float", "float"));

        // Everything a node cannot edit inline is authored as the name of a variable holding it,
        // which is a string, so these share one editor and differ only in what they accept.
        m_dataTypes.push_back(Type(ProgramDataType::Vector3, AZStd::string{}, "Vector3", "AZStd::string"));
        m_dataTypes.push_back(Type(ProgramDataType::EntityId, AZStd::string{}, "EntityId", "AZStd::string"));
        m_dataTypes.push_back(Type(ProgramDataType::Name, AZStd::string{}, "Name", "AZStd::string"));
        m_dataTypes.push_back(Type(ProgramDataType::Quaternion, AZStd::string{}, "Quaternion", "AZStd::string"));
        m_dataTypes.push_back(Type(ProgramDataType::Transform, AZStd::string{}, "Transform", "AZStd::string"));
        m_dataTypes.push_back(Type(ProgramDataType::EntityIdList, AZStd::string{}, "EntityIdList", "AZStd::string"));
    }
} // namespace GOAT::GraphEditor
