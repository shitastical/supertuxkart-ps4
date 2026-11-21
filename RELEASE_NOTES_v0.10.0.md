# SuperTuxKart for PS4: v0.10.0 withdrawn

**Do not install v0.10.0.** The package was built for the development data
layout and did not contain the game assets under `/app0/data`, so it could not
start from a normal install. The package also omitted
`sce_sys/about/right.sprx`.

There is no public package release at the moment. The current source builds a
full, self-contained package with all data and assets inside `/app0/data`; see
the [main README](README.md) for the build and install steps.

Uninstalling the application removes the packaged files under `/app0`. Saves
and configuration under `/data/stk` are separate user data and are not removed
by uninstalling; delete that directory manually for a completely clean removal.

## Known issues

- Online multiplayer and split-screen have not been tested yet.
- Do not change the controls or graphics settings. Those options have not been
  tested on PS4, and changing them may cause issues.
