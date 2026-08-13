---
name: ue-xr-android
description: Use for Unreal Engine OpenXR, VR/MR, Android build, packaging, deployment, device debugging, permissions, signing, Gradle/UAT, performance, or packaged-on-device validation tasks.
---

# UE XR and Android

Resolve every version-sensitive requirement from the project's exact Unreal Engine version and current primary documentation. Never hardcode SDK, NDK, JDK, device-runtime, or store-policy versions.

## Establish the target

1. Record the exact UE semantic version/changelist, installed versus source build, host toolchain, target device/runtime, build configuration, package format, and failing phase.
2. Inspect `.uproject`, enabled XR/platform plugins, `Config/`, `Build.cs`, Target files, Android manifest/UPL additions, and packaging settings.
3. Separate Editor/VR Preview evidence from packaged-device evidence.

Read [references/openxr.md](references/openxr.md) for XR runtime, input, tracking, rendering, and device checks. Read [references/android.md](references/android.md) for toolchain, packaging, deployment, and logs. Load only the relevant file unless the task crosses both surfaces.

## Diagnose in phase order

Classify the failure before editing: toolchain/Turnkey → UBT/UHT → cook → stage/package → install → launch/runtime → rendering/input/performance. Preserve the first causal error and avoid blind deletion of `Intermediate`, `Saved`, `Binaries`, or DDC.

## Safety and verification

- Treat signing keys, credentials, service files, and store configuration as secrets.
- Use a copy or branch for engine/plugin upgrades; saved assets may not downgrade safely.
- Require explicit approval before broad manifest/UPL rewrites, plugin removal, cache deletion, or device data removal.
- Verify Shipping on the target device with install/launch logs, correct runtime/plugin activation, input/tracking, permissions, frame timing, and crash symbols where relevant.
- Report the exact engine/device/build tested, fresh commands/results, and all unverified devices or store paths.
