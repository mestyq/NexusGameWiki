# NexusGameWiki

Guild Wars 2 in-game wiki viewer for Nexus.

## What it does

- live wiki search inside the game
- rich article rendering with sections, tables, infoboxes, links, and images
- right-side contents navigation
- disk-first cache for search results, page HTML, and downloaded wiki images
- recent pages and favorites

## Install

1. Install [Nexus](https://raidcore.gg/gw2/nexus) for Guild Wars 2.
2. Place `NexusGameWiki.dll` in your `Guild Wars 2\\addons\\` folder.
3. Launch the game and open the addon from the Nexus quick-access bar or the configured hotkey.

The addon creates its own `NexusGameWiki\\` support and cache folders automatically inside `Guild Wars 2\\addons\\` the first time it runs.

## Build

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

The script builds the addon and, by default, installs it to the local Guild Wars 2 addons folder.
It also creates a release zip in `release/` containing the single `NexusGameWiki.dll` file for distribution.

## Project layout

- `src/` - addon source
- `vendor/` - Nexus, ImGui, and JSON dependencies
- `build.ps1` - build and install script

## Author

By `mestyq.3204`

## License

This project is licensed under the [MIT License](LICENSE).

Vendored third-party components are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and retain their own upstream license terms.
