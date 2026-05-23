# gaudio Plan

Audio files should start as ordinary `gassets` records. `gaudio` owns typed audio
asset metadata, emitters, channels, playback parameters, and reusable filters.
It can provide SDL3/SDL_mixer edge integration, but core audio policy should stay
plain and testable.

## Responsibilities

1. Consume audio asset records from `gassets`.
2. Store passive metadata such as category, volume defaults, pitch ranges, loop
   flags, and tags.
3. Store audio emitter data: asset id, position, volume scale, direct gain,
   loop count, channel/category, and optional lifetime/follow id supplied by the
   caller.
4. Store playback parameters for positional audio, low-pass, reverb, delay, and
   streamed/static playback.
5. Provide backend-neutral DSP filters where practical.
6. Treat UI sound keys as audio assets so menus can refer to stable ids instead
   of hardcoded paths.
7. Provide an SDL3/SDL_mixer backend edge when implementation starts, because
   these projects are standardizing on SDL3.

## Not Responsible

1. No audio device.
2. No decoding.
3. No mixer.
4. No world raycasts or stage-specific acoustics.
5. No game-specific sound event routing.
6. No renderer or menu dependency.

## Relationship To Other Libraries

1. `gassets` stores generic audio asset records.
2. `gaudio` adds typed audio policy above those records.
3. Gubsy/Splonks can use the SDL3 edge backend or provide their own backend.
4. World acoustics can feed `gaudio` playback parameters, but the world query
   itself stays game-side.

## Splonks Feature Mapping

1. Audio assets include id, name, file path, default volume, and streamed/static
   playback.
2. Playback parameters include volume scale, positional flag, world position,
   loop count, direct gain, low-pass enable/cutoff/wet mix, reverb enable/wet
   mix/feedback/delay, and reverb low-pass cutoff.
3. Current reusable filters include stereo pan, gain, low-pass, and delay/reverb.
4. Current Splonks filter code depends on SDL audio specs at the boundary. In
   `gaudio`, replace that with a tiny backend-neutral block spec such as sample
   rate and channel count. The SDL3 adapter can translate SDL structs into that
   spec.
5. Stage/world acoustics are not generic enough to move wholesale. They should
   become caller code that computes playback/filter parameters and passes them
   into `gaudio`.

## Backend Boundary

1. Core owns data: assets, emitters, channel ids, playback params, filter params,
   and filter processors that operate on float sample blocks.
2. SDL3/SDL_mixer edge code owns actual device, loaded chunks/streams, and
   backend handles.
3. Callers can ignore the SDL3 edge and consume the core data with another audio
   backend if they want.
4. UI, menu, and gameplay code should refer to audio by stable asset id/key, not
   by path.

## Remaining Questions

1. Should channel policy be minimal tags/categories, or should it include a
   mixer-style priority/ducking/voice-limit policy from the start?
2. Should positional attenuation curves live in `gaudio`, or should callers
   compute final gain and filter params before submission?
