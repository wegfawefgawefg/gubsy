# gaudio Plan

Audio files should start as ordinary `gassets` records. A later `gaudio` library
can own directional audio, channels, filtering, emitters, and other
backend-neutral audio policy from Splonks when that boundary is useful.

## Responsibilities

1. Consume audio asset records from `gassets`.
2. Store passive metadata such as category, volume defaults, pitch ranges, loop
   flags, and tags.
3. Provide directional emitter data if it is backend-neutral.
4. Provide channel/filter metadata if it is backend-neutral.
5. Keep backend application explicit and caller-owned.
6. Treat UI sound keys as audio assets so menus can refer to stable ids instead
   of hardcoded paths.

## Not Responsible

1. No audio device.
2. No decoding.
3. No mixer.
4. No listener/world acoustics.
5. No SDL backend in core.

## Relationship To Other Libraries

1. `gassets` stores generic audio asset records.
2. `gaudio` adds typed audio policy above those records.
3. Gubsy/Splonks own playback policy and backend integration.

## Ambiguities

1. Which Splonks audio systems are backend-neutral enough to extract?
2. Are audio emitters generic enough for a library, or game-specific?
3. What parts of filter application can be data-only and what parts require a
   backend adapter?
