# iidxfreq

Changes IIDX song/chart/audio speed from **0.5x to 2x**, with optional pitch preservation. Similar to `FREQ` option in LR2 or `Play Rate` in Infinitas. Judgement windows will be normal and unaffected, as you would expect.

Videos will always play at 1x speed. Not all modes have been tested; stick to single player modes, avoid courses.

Playing at modified speed (other than 1x) blocks sending of scores, as if you were playing with assist options. Play data is still sent. Use at your own risk. Not responsible if you get banned from the server you play on.

## Usage

1. Place the generated DLL and INI from the build output beside Spice.
1. Add `-k iidxfreq.dll` to your usual Spice launch command.
1. In song select, press Home to increase speed, End to decrease speed, and P to toggle pitch preservation.

Requires a recent Spice2x version supporting the Spice SDK.

Save INI files as UTF-8. If a key appears more than once, its last value is used.

## Build

Requires Docker for Linux containers. The first build creates the project's MinGW image and downloads pinned SoundTouch, MinHook, and SimpleIni sources.

```bat
build_docker.bat
```

See [PORTING.md](PORTING.md) for adding DLL versions and [THIRD_PARTY.md](THIRD_PARTY.md) for dependencies and redistribution.
