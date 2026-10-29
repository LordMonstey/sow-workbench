# SoW Workbench

A native C++ editor for Middle-earth: Shadow of War.

- Browse and edit databases, references and strings.
- Inspect archives, models, textures, materials and animations.
- Edit supported world data offline.
- Use the desktop UI, JSON CLI, REST or MCP.

Development preview. Native rendering, full animation controllers and in-game world loading are not fully verified.

## Build

Windows x64, MSVC with C++20, Windows SDK and CMake 3.24+.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target Monstey_Workbench Monstey_WorkbenchCli --parallel
.\build\Release\Monstey_Workbench.exe
```

Choose **Open** and select your own extracted game file.

```powershell
.\build\Release\Monstey_WorkbenchCli.exe --help
.\build\Release\Monstey_WorkbenchCli.exe --catalog
.\build\Release\Monstey_WorkbenchCli.exe --mcp
```

## Tests

```powershell
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Blender scene tests need `-DMONSTEY_TEST_BLEND_TEMPLATE="path/to/startup.blend"`. Private game fixtures are opt-in.

No game assets or decoder DLLs are included. Supply your own Oodle decoder for archives that require it.

MIT for original code. See [third-party notices](THIRD_PARTY_NOTICES.md).
