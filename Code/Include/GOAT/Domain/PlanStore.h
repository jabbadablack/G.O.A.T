#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! Identifies a borrowed block of steps so it can be given back.
    using PlanBlockId = AZ::u32;

    //! Value meaning "these steps are not borrowed", which is true of every baked plan.
    inline constexpr PlanBlockId InvalidPlanBlock = 0;

    //! Where the steps of every plan live.
    //!
    //! An agent's plan is a span into this rather than a copy, which is what removes both the
    //! length limit and the per plan copy. Two regions, because authored and computed plans have
    //! nothing in common but their element type:
    //!
    //! - **Baked**: every option of every authored `plan`, written once when the vocabulary loads
    //!   and immutable afterwards. Nothing is copied at runtime and nothing is ever released.
    //! - **Pooled**: steps a backend computed while the game ran. Borrowed when the plan starts and
    //!   returned when it ends or is aborted, so after warm up a computed plan allocates nothing.
    //!
    //! Pointer stability is the whole design. Both regions hand out raw pointers that must stay
    //! valid for as long as an agent is running the plan, so neither region ever grows a buffer
    //! that is already in use: capacity is decided when a chunk is created and never revisited.
    class PlanStore final
    {
    public:
        AZ_TYPE_INFO(PlanStore, PlanStoreTypeId);
        AZ_CLASS_ALLOCATOR(PlanStore, AZ::SystemAllocator);

        //! Steps handed out by the store. Not owning: the store outlives every span it issues.
        struct Span final
        {
            const ActionRequest* m_steps = nullptr;
            AZ::u32 m_count = 0;
            //! Non zero only for a borrowed block, which Release must be given back.
            PlanBlockId m_block = InvalidPlanBlock;

            bool IsEmpty() const { return m_steps == nullptr || m_count == 0; }
        };

        PlanStore() = default;
        ~PlanStore() = default;

        //! Copies an authored plan's steps in once, for the lifetime of the vocabulary.
        //! The returned span is stable until Rebake, and is never released.
        Span Bake(const ActionRequest* steps, AZ::u32 count);

        //! Drops every baked step. Callers must abort any agent still running a baked plan
        //! first, because their spans point into the memory this releases.
        void ClearBaked();

        //! Borrows room for a plan computed at runtime, copying the steps in.
        //! The returned span carries a block id and must be handed to Release.
        Span Acquire(const ActionRequest* steps, AZ::u32 count);

        //! Returns a borrowed block. Ignores a span that was baked rather than borrowed.
        void Release(Span span);

        //! How many blocks are currently borrowed, which is what proves plans are given back.
        size_t GetBorrowedCount() const;

        //! How many steps are baked, for console output.
        size_t GetBakedCount() const { return m_bakedCount; }

    private:
        //! A run of steps whose address never moves once handed out.
        //! Sized when it is created and never resized, which is what makes that true.
        struct Chunk final
        {
            AZStd::vector<ActionRequest> m_steps;
            size_t m_used = 0;
        };

        //! One borrowable run of steps, recycled rather than freed.
        struct Block final
        {
            AZStd::vector<ActionRequest> m_steps;
            bool m_borrowed = false;
        };

        //! Steps per baked chunk. A plan longer than this gets a chunk of its own, so a span is
        //! always contiguous no matter how long the plan is.
        static constexpr size_t BakedChunkSteps = 256;

        AZStd::vector<AZStd::unique_ptr<Chunk>> m_baked;
        AZStd::vector<AZStd::unique_ptr<Block>> m_blocks;
        AZStd::vector<PlanBlockId> m_freeBlocks;
        size_t m_bakedCount = 0;
    };
} // namespace GOAT
