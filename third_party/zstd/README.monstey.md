# Native decompression-only Zstandard dependency

Pinned official facebook/zstd v1.5.7 tree f8745da6ff1ad1e7bab384bd1f9d742439278e99.
The selected BSD-3-Clause license is retained as LICENSE; COPYING is retained
for upstream's alternative GPLv2 license. PROVENANCE.json records every file.

Compile build/single_file_libs/zstddeclib-in.c directly as C with include path
lib. It already defines decompressor-only settings, disables legacy decoding,
assembly and tracing. No Python amalgamation generator is needed: ordinary C
includes compile the exact official input translation unit. This avoids adding
an encoder or a runtime DLL. Verified with MSVC /TC /MT /W4 /WX.

CMake integration: enable C as a language, add a static library from that one
C file, include lib privately, define _CRT_SECURE_NO_WARNINGS. Link it into the
native Blend codec. Product sources include lib/zstd.h through the target's
public include path. No source or binary from Blender is redistributed here.
