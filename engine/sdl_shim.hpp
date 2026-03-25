#pragma once

/** Goes through sdl input events this frame, updating the gubsy input system state. 
    First translates raw sdl events into states for each device thats plugged in.
        does not know about the player abstraction, just updates per device input states.

    Why: 
        this is the sdl shim. well need different shims later.
        internally we want gubsy inputs, so this is the translation layer.
*/
void update_gubsy_device_inputs_system_from_sdl_events();
