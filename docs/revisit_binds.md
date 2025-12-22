# Revisit: Binds Profiles

Context:
- `data/binds_profiles/default.lisp` now stores a single canonical binds profile with `id 1`.
- `data/player_profiles/user_profiles.lisp` points to that profile via `last_binds_profile`.
- `add_player()` currently reuses whichever binds profile already exists instead of cloning per-user copies. This prevents each player from customizing independently and causes `last_binds_profile` to change every launch.

Follow-up work:
1. Introduce the concept of a **template** binds profile (e.g., profile `id 1`). When a `UserProfile` has `last_binds_profile == -1`, duplicate the template (new id + copied bindings) and persist it to `data/binds_profiles`.
2. Ensure `add_player()` loads the profile, clones if necessary, saves the cloned id back into the player profile, and only then instantiates input bindings.
3. When new user profiles are created, immediately clone the template so every profile starts with its own binds entry (no sharing).
4. Update docs (`docs/PROFILES_GUIDE.md`, etc.) once the cloning behavior is implemented.

Until this revisit is done, there will only be a single shared binds profile on disk. This file tracks the remaining work so we can schedule it after the settings overhaul.

