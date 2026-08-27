#pragma once

#include <GOAT_SmartObject/GOAT_SmartObjectBus.h>

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT_SmartObject
{
    //! Tracks which entities offer which uses, and who is currently using them.
    //!
    //! Deliberately a linear scan: a claim happens on a plan boundary, not per frame, and a
    //! spatial index would have to be kept correct against entities that move. If a project
    //! ever registers enough objects for this to matter, the scan is the thing to replace and
    //! nothing outside this file would change.
    class SmartObjectRegistry final
    {
    public:
        //! Offers an entity to agents, replacing whatever it offered before.
        void Add(AZ::EntityId entity, SmartObjectDescription description);

        //! Withdraws an entity, releasing any agent still holding a slot on it.
        void Remove(AZ::EntityId entity);

        //! Takes a slot on the nearest entity offering @use within @radius of @from.
        SmartObjectClaim Claim(GOAT::AgentId agent, const AZ::Name& use, const AZ::Vector3& from, float radius);

        //! Gives back whatever slot an agent holds. Safe to call when it holds none.
        void Release(GOAT::AgentId agent);

        //! How many slots an entity has left.
        AZ::u32 GetFreeSlots(AZ::EntityId entity) const;

        //! How many entities are registered, for console output.
        size_t GetObjectCount() const { return m_objects.size(); }

    private:
        struct Object
        {
            SmartObjectDescription m_description;
            //! Agents currently holding a slot. Never longer than the capacity.
            AZStd::vector<GOAT::AgentId> m_users;
        };

        //! The world anchor of an entity, or false when it has no transform to read.
        static bool FindAnchor(AZ::EntityId entity, const AZ::Vector3& offset, AZ::Vector3& outAnchor);

        AZStd::unordered_map<AZ::EntityId, Object> m_objects;

        //! An agent holds at most one claim, which is what bounds a leaked slot to one per agent.
        AZStd::unordered_map<GOAT::AgentId, AZ::EntityId> m_claims;
    };
} // namespace GOAT_SmartObject
