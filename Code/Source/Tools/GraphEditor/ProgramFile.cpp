#include <Tools/GraphEditor/ProgramFile.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/std/string/conversions.h>

namespace GOAT::GraphEditor
{
    namespace
    {
        AZ::SerializeContext* Serialization()
        {
            AZ::SerializeContext* context = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(
                context, &AZ::ComponentApplicationRequests::GetSerializeContext);
            AZ_Error("GOAT", context != nullptr, "There is no serialize context to read a program with");
            return context;
        }
    } // namespace

    AuthoredNode DefaultRoot()
    {
        AuthoredNode root;
        root.m_type = "sequence";
        return root;
    }

    bool SaveProgramFile(const AZStd::string& fullPath, const ProgramAsset& asset)
    {
        AZ::SerializeContext* context = Serialization();
        return context != nullptr &&
            AZ::Utils::SaveObjectToFile(fullPath, AZ::DataStream::ST_XML, &asset, context);
    }

    bool LoadProgramFile(const AZStd::string& fullPath, ProgramAsset& asset)
    {
        AZ::SerializeContext* context = Serialization();
        return context != nullptr && AZ::Utils::LoadObjectFromFileInPlace(fullPath, asset, context);
    }

    AZStd::string UnusedProgramPath(const AZStd::string& folder, const AZStd::string& baseName)
    {
        AZStd::string path;
        AZ::StringFunc::Path::ConstructFull(
            folder.c_str(), baseName.c_str(), ProgramAsset::FileExtension, path);

        AZ::IO::FileIOBase* io = AZ::IO::FileIOBase::GetInstance();
        for (int counter = 1; io != nullptr && io->Exists(path.c_str()); ++counter)
        {
            const AZStd::string numbered = baseName + AZStd::to_string(counter);
            AZ::StringFunc::Path::ConstructFull(
                folder.c_str(), numbered.c_str(), ProgramAsset::FileExtension, path);
        }
        return path;
    }
} // namespace GOAT::GraphEditor
