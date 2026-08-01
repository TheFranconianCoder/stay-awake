# Commands

| Command | Description |
|---------|-------------|
| `task check` | Format + lint (clang-format, clang-tidy) |
| `task build` | Check + compile (zig build, ReleaseSmall) |
| `task run` | Build + launch binary |
| `task release VERSION=x.y.z` | Generate notes, tag, build, publish to GitHub |

# Conventions

- **Language:** C23, UNICODE, Win32 API, Windows GUI subsystem
- **Naming:** `camelBack` functions/vars, `PascalCase` types, `UPPER_CASE` constants
- **Format:** LLVM style, 4-space indent, 120 col, Attach braces, left-aligned pointers
- **Lint:** clang-tidy with warnings-as-errors
- **Build:** Zig (build.zig); CMakeLists.txt for IDE analysis only
- **Tools:** mise manages zig, gh, cmake, ninja

# Stack

C → Zig → Win32 → Task/mise
