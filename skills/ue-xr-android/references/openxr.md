# OpenXR checklist

## Runtime and plugins

- Confirm the intended OpenXR runtime and only the required engine/vendor plugins and extensions.
- Verify plugin platform allowlists, module load state, runtime selection, and packaged inclusion.
- Prefer standard OpenXR paths; isolate vendor extensions behind capability checks.

## Input and tracking

- Map supported interaction profiles through Enhanced Input and verify bindings on the real controller/device.
- Check tracking origin, world scale, pose spaces, recenter behavior, controller/hand availability, focus, and session-state transitions.
- Test loss/reacquisition of tracking and runtime pause/resume.

## Rendering and performance

- Verify stereo/RHI configuration, render resolution, swapchain formats, depth submission, mobile multiview/foveation support, and post-processing compatibility for the exact target.
- Measure a packaged build on target hardware after warm-up with a fixed workload and multiple samples. VR Preview is diagnostic evidence, not release proof.

## Device gate

- Confirm permissions, runtime availability, launch logs, headset display, tracking, input, audio, suspend/resume, thermal behavior, and required frame-rate budget.
- Record unsupported or untested runtimes and extensions.
