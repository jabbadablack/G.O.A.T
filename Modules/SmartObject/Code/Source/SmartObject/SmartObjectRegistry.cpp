#include <SmartObject/SmartObjectRegistry.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/std/algorithm.h>

namespace GOAT_SmartObject
{
    void SmartObjectRegistry::Add(AZ::EntityId entity, SmartObjectDescription description)
    {
        AZ_Assert(entity.IsValid(), "A smart object must be offered by a valid entity");
        AZ_Assert(description.m_capacity > 0, "A smart object with no capacity can never be used");

        if (!entity.IsValid() || description.m_capacity == 0)
        {
            AZ_Error("GOAT", false, "Entity %s cannot be a smart object: it has no capacity",
                entity.ToString().c_str());
            return;
        }

        AZ_Warning("GOAT", !description.m_uses.empty(),
            "Smart object %s offers no uses, so no agent can ever claim it", entity.ToString().c_str());

        // Re-registering replaces the offer, and drops whoever was using the old one.
        Remove(entity);

        Object object;
        object.m_description = AZStd::move(description);
        object.m_users.reserve(object.m_description.m_capacity);
        m_objects[entity] = AZStd::move(object);

        AZ_Assert(m_objects.find(entity) != m_objects.end(), "Adding a smart object must leave it findable");
    }

    void SmartObjectRegistry::Remove(AZ::EntityId entity)
    {
        const auto found = m_objects.find(entity);
        if (found == m_objects.end())
        {
            return;
        }

        // Whoever was holding a slot here now holds nothing, or their claim would outlive it.
        for (const GOAT::AgentId agent : found->second.m_users)
        {
            m_claims.erase(agent);
        }

        m_objects.erase(found);

        AZ_Assert(m_objects.find(entity) == m_objects.end(), "Removing a smart object must leave nothing behind");
    }

    bool SmartObjectRegistry::FindAnchor(AZ::EntityId entity, const AZ::Vector3& offset, AZ::Vector3& outAnchor)
    {
        AZ_Assert(entity.IsValid(), "An anchor is only read from a valid entity");

        // Checked rather than inferred from the result: an entity with no transform handler
        // leaves the identity in place, which would silently anchor everything at the origin.
        if (!AZ::TransformBus::HasHandlers(entity))
        {
            AZ_Error("GOAT", false, "Smart object %s has no transform, so it has no anchor",
                entity.ToString().c_str());
            return false;
        }

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(transform, entity, &AZ::TransformInterface::GetWorldTM);
        outAnchor = transform.TransformPoint(offset);
        return true;
    }

    SmartObjectClaim SmartObjectRegistry::Claim(
        GOAT::AgentId agent, const AZ::Name& use, const AZ::Vector3& from, float radius)
    {
        AZ_Assert(!agent.IsNull(), "A null agent cannot claim a smart object");
        AZ_Assert(!use.IsEmpty(), "A smart object is always claimed by the name of a use");
        AZ_Assert(radius > 0.0f, "A search radius must be positive");

        SmartObjectClaim claim;
        if (agent.IsNull() || use.IsEmpty())
        {
            return claim;
        }

        // One claim per agent, so taking a new one gives back the old.
        Release(agent);
        AZ_Assert(m_claims.find(agent) == m_claims.end(), "Claiming must start from an agent holding nothing");

        float bestDistanceSq = radius * radius;
        AZ::EntityId bestEntity;
        AZ::Vector3 bestAnchor = AZ::Vector3::CreateZero();

        for (auto& [entity, object] : m_objects)
        {
            if (object.m_users.size() >= object.m_description.m_capacity)
            {
                continue;
            }

            const auto& uses = object.m_description.m_uses;
            if (AZStd::find(uses.begin(), uses.end(), use) == uses.end())
            {
                continue;
            }

            AZ::Vector3 anchor = AZ::Vector3::CreateZero();
            if (!FindAnchor(entity, object.m_description.m_anchorOffset, anchor))
            {
                continue;
            }

            const float distanceSq = from.GetDistanceSq(anchor);
            if (distanceSq >= bestDistanceSq)
            {
                continue;
            }

            bestDistanceSq = distanceSq;
            bestEntity = entity;
            bestAnchor = anchor;
        }

        if (!bestEntity.IsValid())
        {
            AZLOG(GoatSmartObject, "GOAT: agent %u found nothing offering '%s' within %.1f m",
                agent.GetIndex(), use.GetCStr(), radius);
            return claim;
        }

        m_objects[bestEntity].m_users.push_back(agent);
        m_claims[agent] = bestEntity;

        claim.m_entity = bestEntity;
        claim.m_anchor = bestAnchor;

        AZ_Assert(m_claims.find(agent) != m_claims.end(), "A successful claim must be recorded against the agent");
        AZ_Assert(m_objects[bestEntity].m_users.size() <= m_objects[bestEntity].m_description.m_capacity,
            "A smart object must never hold more users than its capacity");

        AZLOG(GoatSmartObject, "GOAT: agent %u claimed '%s' on entity %s",
            agent.GetIndex(), use.GetCStr(), bestEntity.ToString().c_str());
        return claim;
    }

    void SmartObjectRegistry::Release(GOAT::AgentId agent)
    {
        const auto claim = m_claims.find(agent);
        if (claim == m_claims.end())
        {
            return;
        }

        const auto object = m_objects.find(claim->second);
        AZ_Assert(object != m_objects.end(), "A recorded claim must point at an object that exists");
        if (object != m_objects.end())
        {
            auto& users = object->second.m_users;
            users.erase(AZStd::remove(users.begin(), users.end(), agent), users.end());
        }

        m_claims.erase(claim);

        AZ_Assert(m_claims.find(agent) == m_claims.end(), "Releasing must leave the agent holding nothing");
    }

    AZ::u32 SmartObjectRegistry::GetFreeSlots(AZ::EntityId entity) const
    {
        const auto found = m_objects.find(entity);
        if (found == m_objects.end())
        {
            return 0;
        }

        const AZ::u32 used = static_cast<AZ::u32>(found->second.m_users.size());
        AZ_Assert(used <= found->second.m_description.m_capacity,
            "A smart object must never hold more users than its capacity");

        return found->second.m_description.m_capacity - used;
    }
} // namespace GOAT_SmartObject
