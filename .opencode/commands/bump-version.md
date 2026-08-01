---
description: Bump version constants in src/app_state.h and resource.rc
---

Read src/app_state.h and resource.rc to find the current version. The user will provide the new version as MAJOR.MINOR.PATCH. Update these locations:

1. src/app_state.h: three lines APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH
2. resource.rc: FILEVERSION, PRODUCTVERSION (comma-separated), and VALUE "FileVersion", VALUE "ProductVersion" (dot-separated with .0 suffix)

After updating, run `task build` to verify the build compiles. Do not commit.
