// Stops the game from sending StopHaptics every frame to idle controllers.
//
// Once a hand has played basic rumble, RumbleHapticFeedbackPlayer.UpdateRumbles keeps that hand in its
// table for good and calls StopHaptics for it on every LateUpdate while no rumble is active. In testing,
// those repeats silence advanced haptics on that hand until the game restarts. The exact mechanism is
// inside the runtime and has not been observed directly.
//
// UpdateRumbles is the only managed caller of both hooked methods, so this only needs to remember whether
// a pulse was sent since the last stop. Everything runs on the main thread.

#include "Haptics.hpp"
#include "logging.hpp"
#include "beatsaber-hook/shared/hooking.hpp"
#include "GlobalNamespace/UnityXRHelper.hpp"
#include "UnityEngine/XR/XRNode.hpp"

namespace {
    using Node = UnityEngine::XR::XRNode;

    bool leftPlaying = false;
    bool rightPlaying = false;

    bool* PlayingFlag(Node node) {
        return node == Node::LeftHand ? &leftPlaying : node == Node::RightHand ? &rightPlaying : nullptr;
    }

    MAKE_HOOK_MATCH(UnityXRHelper_TriggerHapticPulse, &GlobalNamespace::UnityXRHelper::TriggerHapticPulse,
                    void, GlobalNamespace::UnityXRHelper* self, Node node,
                    float duration, float strength, float frequency) {
        UnityXRHelper_TriggerHapticPulse(self, node, duration, strength, frequency);
        if (auto playing = PlayingFlag(node)) *playing = true;
    }

    MAKE_HOOK_MATCH(UnityXRHelper_StopHaptics, &GlobalNamespace::UnityXRHelper::StopHaptics,
                    void, GlobalNamespace::UnityXRHelper* self, Node node) {
        if (auto playing = PlayingFlag(node)) {
            if (!*playing) return;
            *playing = false;
        }
        UnityXRHelper_StopHaptics(self, node);
    }
}

void BSFixes::InstallHapticsFix() {
    INSTALL_HOOK(Logger, UnityXRHelper_TriggerHapticPulse);
    INSTALL_HOOK(Logger, UnityXRHelper_StopHaptics);
}
