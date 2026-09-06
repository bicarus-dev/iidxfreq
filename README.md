# iidxfreq

**Play on offline servers only, out of precaution!**

Changes IIDX song/chart/audio speed from **0.5x to 2x**, with optional pitch preservation. Similar to `FREQ` option in LR2 or `Play Rate` in Infinitas. Judgement windows will be normal and unaffected, as you would expect.

FREQ is available in Standard, Step Up, and Premium Free. Other modes run at 1x without changing your pending rate.

Videos always play at 1x; multiplayer remains untested.

This hook will try its best to tell the server to not save anything; scores are sent with "no save", DAN classes are requested to not upgrade, and so on. However, there may be gaps that this hook missed. Use at your own risk. Not responsible if you get banned from the server you play on. It's recommended that you stick to an offline server and assume none of your scores will save.

## Usage

1. Place the generated DLL and INI from the build output beside Spice.
1. Add `-k iidxfreq.dll` to your usual Spice launch command.
1. In song select, press Home to increase speed, End to decrease speed, and P to toggle pitch preservation.

Requires a recent Spice2x version supporting the Spice SDK.

## Supported LDJ versions

See https://github.com/bicarus-dev/iidxfreq/blob/main/src/versions/registry.h

## Build

Requires Docker for Linux containers. The first build creates the project's MinGW image and downloads pinned SoundTouch, MinHook, and SimpleIni sources.

```bat
build_docker.bat
```

See [PORTING.md](PORTING.md) for adding DLL versions and [THIRD_PARTY.md](THIRD_PARTY.md) for dependencies and redistribution.
