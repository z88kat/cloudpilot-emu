#ifndef _PATCHES_H_
#define _PATCHES_H_

#include "patch_dispatch.h"
#include "syscall_dispatch.h"

struct ArmCpu;
struct PatchContext;
struct SystemState;

#define PATCH_FEATURE_TOUCH_TRANSLATION 0x01
#define PATCH_FEATURE_NO_SLEEP 0x02
#define PATCH_FEATURE_ALL 0xffffffff

// Linear mapping between the raw pen coordinates that the OS passes to the HAL and screen
// coordinates: raw = rawOffset + (screen - screenOffset) * num / den
struct PenAxisTranslation {
    int32_t rawOffset;
    int32_t screenOffset;
    int32_t num;
    int32_t den;
};

struct PenTranslation {
    struct PenAxisTranslation x;
    struct PenAxisTranslation y;
};

// Touch translation is only installed if penTranslation is not null
struct PatchContext* registerPatches(struct PatchDispatch* patchDispatch,
                                     struct SyscallDispatch* syscallDispatch, struct ArmCpu* cpu,
                                     struct SystemState* systemState,
                                     uint32_t features = PATCH_FEATURE_ALL,
                                     const struct PenTranslation* penTranslation = nullptr);

#endif  // _PATCHES_H_
