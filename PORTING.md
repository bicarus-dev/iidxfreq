# Porting iidxfreq

This guide walks through adding support for another version of the IIDX game DLL.

## 1. Identify the new build

The profile identifier follows Spice's patch naming convention:

`MODEL-timestamp_entrypoint` e.g., ``LDJ-69ddf5f8_af6cdc``

You can get this from launching the game in spice2x and looking at patcher logs.

## 2. Start a new version header

Copy [src/versions/LDJ-69ddf5f8_af6cdc.h](src/versions/LDJ-69ddf5f8_af6cdc.h) into the same directory, naming the copy after your new identifier with a `.h` extension. Keep the original so that the existing build remains supported.

In the copy, change:

- `pe_identifier`: the identifier from step 1.
- The profile variable: give it a unique name based on the new identifier.

The remaining version-specific values are the eight hook RVAs and their entry bytes, the mode getter RVA and entry bytes, and the wave vtable RVA. Update them using the steps below, and leave the new profile out of the registry until they are confirmed.

## 3. Find the matching hook functions

Keep a supported DLL open beside the new one to compare callers and constants. Use the search clues below to locate equivalent code in the new build.

### Chart and song setup

- **`chart_convert`:** Look for a loop stepping through 8-byte records, reading a timestamp at `+0`, event type at `+4`, and a 16-bit value at `+6`. Useful fingerprints are type `4` for BPM, type `6` as the end marker, and types `0`, `1`, `100`, `101` for notes/holds. The known converter also uses timestamp arithmetic equivalent to `trunc(ms / (1000 / R) + 0.4)`.
- **`gameplay_setup`:** Once the converter is found, walk upward through the chart-loading callers to the selected-song setup that loads both players' charts. Use this caller relationship rather than searching for the setup function's prologue.
- **`gameplay_mode`:** Find the menu label table containing `STANDARD`, `CLASS`, `STEP UP`, and `PREMIUM FREE`. Follow menu confirmation through its index-to-mode conversion and the setter storing that mode. Locate the no-argument getter returning the same global as a 32-bit value. Confirm Standard is `0`, Step Up is `5`, and Premium Free is `6`, and that the state is set before song setup. Record the getter's RVA and first 15 bytes separately from `hooks`; it is called, not detoured.

### Audio bank and sample loading

- **Find the bank dispatcher first:** Search for the `S3P0` magic check, including immediate constants if it has no string xref. Follow the dispatch branches to the S3P and 2dx bank loaders. An `SD9\0` check identifies a single-sample SD9 loader, not the 2dx bank loader.
- **`decode` / `s3p_decode`:** Follow calls inside each bank loader's sample loop. The 2dx path reaches a RIFF/ACM decoder; the S3P path reaches a Media Foundation decoder. RIFF checks and ACM/Media Foundation import xrefs help narrow candidates. Confirm that both return the same two-pointer shared-wave wrapper through their first argument.
- **`loop_voice`:** Follow the call immediately downstream of sample decoding into the audio device's voice factory, including indirect calls. In the 2dx loader, look for header byte offsets divided by block alignment before being passed as frame bounds. The S3P path passes `-1` as the end bound. Distinguish the device factory from the lower-level voice constructor it calls.
- **`sound_load`:** Walk upward from the bank dispatcher through file-loading callers to the queued worker receiving `(group, path)`. To distinguish it from the queueing wrapper, inspect the callers: gameplay queues group `2`, while preview queues group `4`. Check the worker's return in assembly if the decompiler omits it: decoding must finish before it returns, with zero on success.

### Score request paths

- **`dispatch`:** Search for `music.reg` and `music.nosave` in strings/request descriptors, then follow references to their common request lookup. Look for a table traversal with 72-byte entries. Request IDs `29` and `34` are useful caller-side anchors. Find the lookup before ticket allocation and XRPC, not a request-specific serializer.
- **`invalid_play`:** Trace the callers issuing those two requests back to the result-screen branch that chooses between them. Look for a predicate whose nonzero result selects no-save and whose zero result selects normal registration. The predicate before that branch is the hook target, not either registration helper.

### Confirm and record each match

Compare argument order, widths, and return values against the function-pointer declarations in [src/game_hooks.cpp](src/game_hooks.cpp). In particular, the RIFF decoder takes a `size_t` size while the S3P decoder takes `uint32_t`. Use caller assembly to resolve ambiguous decompiler prototypes.

For each confirmed match, replace its RVA and byte array in your new header:

1. Subtract the new DLL's image base from the function's address to obtain its RVA.
2. Copy the first 15 bytes at that function's entry into its byte array. These are exact bytes, not a wildcard signature.

Do not reuse a file offset as an RVA. Do not identify functions by those 15 bytes alone: many functions have identical prologues. The byte comparison is a startup guard after you have identified the function.

## 4. Find the wave vtable

Search RTTI names for `WaveDataImpl`, then follow references to the vtable and constructor. If RTTI is unavailable, trace allocations in both decoders to the constructor that writes the same vtable address into the decoded object.

To distinguish it from the shared-pointer control block or audio-device vtable, inspect its destructor for a call to the stored free callback with the owned allocation. Both decoder paths must lead to this same object type.

Subtract the DLL image base from the vtable address and set `.wave.vtable_rva` in the new version header. This is the address of the vtable itself, not one of its function entries.

## 5. Register and build

In [src/versions/registry.h](src/versions/registry.h), include your new header and append the address of its profile variable to `profiles`. Keep all existing entries. The array size is inferred, so there is no count to update and no CMake change is needed for a new header.

Build from the project directory:

```bat
build_docker.bat
```

## 6. Verify the port in game

It's recommended that you test in some offline server first (e.g., Asphyxia) to double check that score blocking works as intended.
