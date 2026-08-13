# Android delivery checklist

## Toolchain

- Resolve SDK/NDK/JDK and host prerequisites through the exact UE version's Turnkey output and current Epic/platform documentation.
- Record architecture, RHI, texture formats, package/application ID, versioning, build configuration, and plugin platform support.

## Build and package

- Classify UBT/UHT, cook, stage, Gradle, signing, bundle, and asset-delivery failures separately.
- Inspect `Build.cs`, Target files, Android settings, manifest merges, UPL/APL contributions, Gradle output, and the first causal AutomationTool error.
- Validate ARM64, selected graphics APIs, cooked content, AAB/APK choice, expansion or asset-delivery configuration, and Shipping symbol retention as required by the target.

## Deploy and debug

- Verify device visibility and supported ABI, install/update behavior, package identity, permissions, launch activity, and runtime libraries.
- Capture filtered `adb logcat`, native crash/tombstone or symbolized callstack, Unreal log, device/GPU/OS identity, and reproduction steps.
- Test clean install, upgrade path when relevant, cold launch, background/resume, network loss, storage pressure, and thermal/performance behavior.

## Release gate

- Keep signing material and service credentials out of logs and source control.
- Validate the Shipping package on every claimed device/runtime class. Do not infer store or device success from Editor, Development, emulator, or installation alone.
