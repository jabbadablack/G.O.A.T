#include <Core/Application/BlackboardSystem.h>
#include <Core/Scripting/AgentScriptContext.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Math/MathReflection.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/IO/SystemFile.h>
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

        //! Runs the authoring vocabulary as it ships, so what these tests exercise is the real
        //! file rather than a restatement of it.
        bool LoadVocabulary()
        {
            AZ::IO::SystemFile file;
            if (!file.Open(GOAT_VOCABULARY_SOURCE, AZ::IO::SystemFile::SF_OPEN_READ_ONLY))
            {
                ADD_FAILURE() << "could not open " << GOAT_VOCABULARY_SOURCE;
                return false;
            }

            AZStd::string source(static_cast<size_t>(file.Length()), '\0');
            file.Read(source.size(), source.data());
            file.Close();
            return m_script->Execute(source.c_str());
        }

        //! Asks a behaviour for a number the way LuaDispatch::MeasureBehavior does.
        bool Measure(const char* behavior, AZStd::span<const float> values, double& outValue)
        {
            AZ::ScriptDataContext call;
            EXPECT_TRUE(m_script->Call("GOAT_Measure", call)) << "GOAT_Measure must be callable";

            call.PushArg(AZStd::string(behavior));
            call.PushArg(AZStd::string("score"));
            call.PushArg(0);
            call.PushArg(m_context);
            for (const float value : values)
            {
                call.PushArg(static_cast<double>(value));
            }

            EXPECT_TRUE(call.CallExecute());
            EXPECT_GE(call.GetNumResults(), 2);

            bool answered = false;
            outValue = 0.0;
            call.ReadResult(0, outValue);
            call.ReadResult(1, answered);
            return answered;
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

    //! The numbers a scorer is handed cross as ordinary arguments, so this is the one place
    //! that proves they arrive at all, in order, and as many as were pushed.
    TEST_F(ScriptBoundaryFixture, Measure_HandsALuaBehaviourItsNumbersAndReadsOneBack)
    {
        ASSERT_TRUE(LoadVocabulary());
        ASSERT_TRUE(Run(R"(
            behavior "Weigh" {
                score = function(me, ctx, considered)
                    local total = #considered * 100
                    for index, value in ipairs(considered) do
                        total = total + value * index
                    end
                    return total
                end,
            }
        )"));

        const float values[] = { 0.5f, 0.25f };
        double measured = 0.0;
        EXPECT_TRUE(Measure("Weigh", values, measured));

        // Three numbers, not two, would still have answered; the count and the order are what
        // a scorer reads its own considerations by.
        EXPECT_DOUBLE_EQ(measured, 201.0);
    }

    //! A behaviour that is not there and one that answers zero mean different things: the first
    //! is a name nobody declared, and treating it as a score would silently veto whatever asked.
    TEST_F(ScriptBoundaryFixture, Measure_TellsAMissingBehaviourApartFromAScoreOfZero)
    {
        ASSERT_TRUE(LoadVocabulary());
        ASSERT_TRUE(Run(R"(
            behavior "Nothing" { score = function() return 0.0 end }
            behavior "NoScore" { tick = function() return SUCCESS end }
        )"));

        double measured = -1.0;
        EXPECT_TRUE(Measure("Nothing", {}, measured));
        EXPECT_DOUBLE_EQ(measured, 0.0);

        measured = -1.0;
        EXPECT_FALSE(Measure("Absent", {}, measured));
        EXPECT_DOUBLE_EQ(measured, 0.0);

        // Declared, but with nothing to ask for a number, which is the same answer as absent.
        measured = -1.0;
        EXPECT_FALSE(Measure("NoScore", {}, measured));
    }

    //! What the compiler asks before it accepts a choice naming a scorer.
    TEST_F(ScriptBoundaryFixture, HasBehavior_AnswersForWhatWasDeclared)
    {
        ASSERT_TRUE(LoadVocabulary());
        ASSERT_TRUE(Run(R"(behavior "Weigh" { score = function() return 1.0 end })"));

        ASSERT_TRUE(Run("declared = GOAT_HasBehavior('Weigh')"));
        ASSERT_TRUE(Run("absent = GOAT_HasBehavior('Weighh')"));

        bool declared = false;
        bool absent = true;
        ASSERT_TRUE(m_script->ReadGlobal("declared", declared));
        ASSERT_TRUE(m_script->ReadGlobal("absent", absent));
        EXPECT_TRUE(declared);
        EXPECT_FALSE(absent);
    }
} // namespace GOAT
