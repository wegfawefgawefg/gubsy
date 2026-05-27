# CI and Release Policy

Gubsy is a code-first library and tool repo. Normal development should not wait
on GitHub-hosted runners.

## Normal development

- Build and test locally before committing.
- Use local scripts such as `./scripts/build.sh`, `ctest --test-dir build
  --output-on-failure`, and focused smoke scripts for the change being made.
- Use `./scripts/validate_local.sh all` when a Gubsy task needs a recorded
  local evidence log for developer, consumer, room server, lobby, and
  developer/tooling package validation.
- Pushes and pull requests should not automatically run package builds.

## Remote checks

- The GitHub package workflow is manual-only.
- Trigger it when we explicitly want remote platform confidence, such as before
  a dependency packaging change is considered done.
- Expected remote platform targets are Linux, macOS, Windows, Android, and iOS,
  but mobile targets should be added only when their toolchains and signing
  paths are intentionally configured.
- Do not treat the manual workflow as the normal edit/compile/test loop.

## Release artifacts

Gubsy is not the shipped game. If we keep packaged Gubsy artifacts, they are
developer/tooling artifacts, not the default release path. The game release
pipeline belongs in Splonks.
