#include <Core/Application/BlackboardSystem.h>
#include <Core/Scripting/AgentScriptContext.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Math/MathReflection.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    //! Runs real Lua against the real reflection.
    //!
    //! Everything else tests the C++ side of the script boundary and takes the Lua side on
    //! trust, which is precisely where a handle that reads back as nothing would hide: the C++
    //! contract can be perfect while the value never survives the round trip.
    class ScriptBoundaryFixture : public UnitTest::LeakDetectionFixture
    {
    protected:
        void SetUp() override
        {
            UnitTest::LeakDetectionFixture::SetUp();
            AZ::NameDictionary::Create();

            m_blackboard = AZStd::make_unique<BlackboardSystem>();
            m_agent = AgentId(0, 1);
            m_blackboard->CreateAgentBlackboard(m_agent);

            m_behavior = aznew AZ::BehaviorContext();

            // The context's methods take vectors and entity ids, and a BehaviorContext refuses
            // to expose a method whose argument types it does not know.
            AZ::MathReflect(m_behavior);
            AZ::Entity::Reflect(m_behavior);

            AgentScriptContext::Reflect(m_behavior);

            m_script = aznew AZ::ScriptContext();
            m_script->BindTo(m_behavior);

            m_context.Bind(m_agent, AZ::EntityId(1), m_blackboard.get());
        }

        void TearDown() override
        {
            m_context.Unbind();
            delete m_script;
            delete m_behavior;
            m_blackboard.reset();
            AZ::NameDictionary::Destroy();
            UnitTest::LeakDetectionFixture::TearDown();
        }

        //! Runs a snippet with the bound context available to it as `ctx`.
        bool Run(const char* lua)
        {
            m_script->AddReplaceGlobal("ctx", m_context);
            return m_script->Execute(lua);
        }

        BlackboardKey Declare(const char* name, BlackboardScope scope, BlackboardType type)
        {
            const auto declared = m_blackboard->Declare(AZ::Name(name), scope, type);
            EXPECT_TRUE(declared.IsSuccess());
            return declared.GetValue();
        }

        AgentId m_agent;
        AZStd::unique_ptr<BlackboardSystem> m_blackboard;
        AZ::BehaviorContext* m_behavior = nullptr;
        AZ::ScriptContext* m_script = nullptr;
        AgentScriptContext m_context;
    };

    //! The whole contract, through Lua: look a variable up, keep what came back, read and write
    //! through it. This is what every rewritten script now does.
    TEST_F(ScriptBoundaryFixture, Key_SurvivesTheRoundTripThroughLua)
    {
        const BlackboardKey speed = Declare("speed", BlackboardScope::Agent, BlackboardType::Float);

        ASSERT_TRUE(Run("handle = ctx:Key('speed')"));
        ASSERT_TRUE(Run("ctx:SetNumber(handle, 3.5)"));

        const float* value = m_blackboard->Find<float>(speed, m_agent);
        ASSERT_NE(value, nullptr);
        EXPECT_FLOAT_EQ(*value, 3.5f);

        ASSERT_TRUE(Run("readBack = ctx:GetNumber(handle)"));
        double readBack = 0.0;
        ASSERT_TRUE(m_script->ReadGlobal("readBack", readBack));
        EXPECT_DOUBLE_EQ(readBack, 3.5);
    }

    //! A handle kept in an upvalue across calls has to keep working, which is the shape every
    //! script uses: resolve once in a helper, read through it on every later tick.
    TEST_F(ScriptBoundaryFixture, Key_KeepsWorkingWhenHeldAcrossCalls)
    {
        Declare("count", BlackboardScope::Agent, BlackboardType::Int);

        ASSERT_TRUE(Run(R"(
            local handle
            function bump()
                if (handle or 0) ~= 0 then
                else
                    handle = ctx:Key('count')
                end
                ctx:SetNumber(handle, ctx:GetNumber(handle) + 1)
            end
        )"));

        ASSERT_TRUE(Run("bump() bump() bump() total = ctx:GetNumber(ctx:Key('count'))"));

        double total = 0.0;
        ASSERT_TRUE(m_script->ReadGlobal("total", total));
        EXPECT_DOUBLE_EQ(total, 3.0);
    }

    //! The first global bool is packed key zero, which is also what a missing Lua argument
    //! becomes. Reading through nothing must not land on it.
    TEST_F(ScriptBoundaryFixture, Key_ZeroIsNotAHandle)
    {
        Declare("flag", BlackboardScope::Global, BlackboardType::Bool);

        ASSERT_TRUE(Run("ctx:SetBool(ctx:Key('flag'), true)"));
        ASSERT_TRUE(Run("stray = ctx:GetBool(0)"));

        bool stray = true;
        ASSERT_TRUE(m_script->ReadGlobal("stray", stray));
        EXPECT_FALSE(stray);
    }

    //! The path the gem actually uses: a behaviour is a function stored in a table, and C++
    //! hands it the context as an argument rather than leaving one in a global. If a handle
    //! survives a global but not an argument, every script in the gem is broken and every test
    //! above still passes -- which is exactly the gap this closes.
    TEST_F(ScriptBoundaryFixture, Key_SurvivesWhenTheContextArrivesAsAnArgument)
    {
        Declare("speed", BlackboardScope::Agent, BlackboardType::Float);

        ASSERT_TRUE(Run(R"(
            local handle
            -- A plain global: ScriptContext::Call resolves a name with lua_getglobal, so a
            -- dotted one never resolves.
            behaviourTick = function(me, ctx, dt)
                if (handle or 0) ~= 0 then else handle = ctx:Key('speed') end
                ctx:SetNumber(handle, ctx:GetNumber(handle) + 1)
                return handle
            end
        )"));

        // Called the way LuaDispatch calls a behaviour: the context pushed as an argument.
        for (int i = 0; i < 3; ++i)
        {
            AZ::ScriptDataContext call;
            ASSERT_TRUE(m_script->Call("behaviourTick", call)) << "the behaviour must be callable";
            call.PushArg(0);
            call.PushArg(m_context);
            call.PushArg(0.0f);
            ASSERT_TRUE(call.CallExecute());

            double handle = 0.0;
            ASSERT_GE(call.GetNumResults(), 1);
            ASSERT_TRUE(call.ReadResult(0, handle));
            EXPECT_NE(handle, 0.0) << "ctx:Key answered nothing when the context arrived as an argument";
        }

        ASSERT_TRUE(Run("total = ctx:GetNumber(ctx:Key('speed'))"));
        double total = 0.0;
        ASSERT_TRUE(m_script->ReadGlobal("total", total));
        EXPECT_DOUBLE_EQ(total, 3.0);
    }

    //! An undeclared name answers with a handle that reads as nothing rather than one that
    //! happens to address a slot.
    TEST_F(ScriptBoundaryFixture, Key_AnswersZeroForAnUndeclaredName)
    {
        // Looking up a name nothing declares is reported; how many traces that takes is the
        // trace system's business, not this test's.
        AZ_TEST_START_TRACE_SUPPRESSION;
        ASSERT_TRUE(Run("missing = ctx:Key('never_declared')"));
        AZ_TEST_STOP_TRACE_SUPPRESSION_NO_COUNT;

        double missing = -1.0;
        ASSERT_TRUE(m_script->ReadGlobal("missing", missing));
        EXPECT_DOUBLE_EQ(missing, 0.0);
    }
} // namespace GOAT
