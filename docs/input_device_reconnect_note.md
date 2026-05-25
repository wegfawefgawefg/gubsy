# Input Device Reconnect Note

This is a deferred design note. The current behavior is acceptable for now, but
controller reconnect UX should be revisited before treating lobby input
assignment as finished.

## Current Behavior

- Keyboard and mouse are aggregate sources with id `0`.
- Gubsy treats keyboard and mouse as always present.
- Gamepads are represented by the SDL device index currently used by the open
  controller.
- Player 1 is assigned the first gamepad at startup if no gamepad is already
  assigned.
- A hotplugged gamepad is assigned to player 1 if player 1 has no gamepad.
- When a gamepad is unplugged, assignments for that gamepad id are removed.

## Problem

Immediate removal is simple, but it is not ideal for battery dropouts or loose
cables. If a controller disconnects briefly, the player may have to reassign it.

The deeper issue is identity. SDL device indexes are not stable enough to model
"the same controller" across unplug and replug. Replug order can change the
index, and multiple identical controllers make naive matching ambiguous.

## Preferred Future Shape

- Separate assignment intent from availability.
- Keep an assignment visible when a device disconnects.
- Mark unavailable devices as disconnected in the lobby UI.
- Reattach automatically when the same physical controller is likely found.
- Remove assignments only when the user explicitly unassigns the device.

## Identity Direction

A future `InputDeviceIdentity` should probably include:

- SDL controller GUID.
- Device name.
- Vendor/product info if SDL exposes it in the target SDL version.
- A runtime handle for the currently connected instance.
- A fallback slot/order value only for ambiguous cases.

This should not be built on raw SDL device index alone.

## Non-Goals For Now

- Do not overbuild persistent device identity yet.
- Do not require users to manage keyboard/mouse devices individually.
- Do not block current lobby/input work on this.
