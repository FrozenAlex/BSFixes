// Fixes a main-thread deadlock in Beat Saber 1.45.3's Oculus access-token request.
//
// OculusStudios.Platform.Oculus.PlatformUser.InternalGetAccessTokenAsync is compiled from roughly:
//
//     messageTask = Users.GetAccessToken().<as Task>();
//     timeoutTask = Task.Delay(TimeSpan.FromSeconds(20));
//     completedFirst = Task.WhenAny(messageTask, timeoutTask);   // not awaited: the WhenAny task itself
//     await completedFirst;
//     if (completedFirst == timeoutTask) throw new TimeoutException(...);
//     return messageTask.Result...;
//
// completedFirst is the WhenAny task, never the winner, so the timeout check never matches. When the
// token request takes longer than 20 s, `messageTask.Result` blocks Unity's main thread. Oculus platform
// responses are delivered on that thread, so the request can never complete and the game freezes.
//
// This hook runs when the state machine resumes after that await and replaces completedFirst with the
// task that actually finished first. The game's own timeout then works: it throws its TimeoutException
// after 20 s instead of blocking.

#include "main.hpp"
#include "Haptics.hpp"

#include "logging.hpp"
#include "scotland2/shared/modloader.h"

#include "beatsaber-hook/shared/hooking.hpp"
#include "beatsaber-hook/shared/api.hpp"

#include "OculusStudios/Platform/Oculus/PlatformUser.hpp"
#include "System/Runtime/CompilerServices/TaskAwaiter.hpp"
#include "System/Threading/Tasks/Task.hpp"
#include "System/Threading/Tasks/Task_1.hpp"

using StateMachine = OculusStudios::Platform::Oculus::PlatformUser__InternalGetAccessTokenAsync_d__12;
using System::Threading::Tasks::Task;
using WhenAnyTask = System::Threading::Tasks::Task_1<Task*>;

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

// <>1__state while suspended at `await completedFirst` (the method's only await).
static constexpr int32_t awaitingCompletedFirst = 0;

MAKE_HOOK_MATCH(PlatformUser_InternalGetAccessTokenAsync_MoveNext, &StateMachine::MoveNext, void, StateMachine* self) {
    auto whenAny = self->__u__1.m_task;
    if (self->__1__state == awaitingCompletedFirst && whenAny && self->_completedFirst_5__4 == whenAny && whenAny->IsCompleted) {
        auto winner = reinterpret_cast<WhenAnyTask*>(whenAny)->m_result;
        self->_completedFirst_5__4 = winner;
        if (winner == self->_timeoutTask_5__3)
            WARNING("Access-token request timed out after 20 s; letting the game handle the timeout instead of blocking");
        else
            DEBUG("Access-token request finished before the timeout");
    }
    PlatformUser_InternalGetAccessTokenAsync_MoveNext(self);
}

MOD_EXTERN_FUNC void setup(CModInfo* info) noexcept {
    *info = modInfo.to_c();
}

// late_load runs before the game sends its first access-token request, and the hook only acts when the
// state machine resumes, at least 20 s or one completed request later.
MOD_EXTERN_FUNC void late_load() noexcept {
    i2c::functions::initialize();
    INSTALL_HOOK(Logger, PlatformUser_InternalGetAccessTokenAsync_MoveNext);
    BSFixes::InstallHapticsFix();
}
