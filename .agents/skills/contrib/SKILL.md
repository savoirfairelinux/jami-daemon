---
name: contrib
description: Build or work on Jami core dependencies (contrib system)
---

# Contrib system

Jami-core (the daemon) uses the "contrib" system to manage its dependencies.
The contrib system allows building required dependencies as needed (ie when not available in the system).

# Build a contrib

Contribs are built in `contrib/build-{arch}` and installed in `contrib/{arch}` using rules in `contrib/{contrib-name}/rules.mak`.

They are built automatically by CMake when configuring the project (when BUILD_CONTRIB is not disabled), but they can also be built manually by running `make .{contrib-name}` from the contrib build directory.

# Notes for Windows/MSVC

On Windows with MSVC, the contribs are built using the independent pywinmake system, using rules in `contrib/{contrib-name}/package.json`.
