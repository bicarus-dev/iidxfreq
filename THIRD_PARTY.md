# Third-party components

## SoundTouch 2.3.3

Source: https://codeberg.org/soundtouch/soundtouch

Pinned commit: `e83424d5928ab8513d2d082779c275765dee31b9`.

SoundTouch is licensed under LGPL-2.1-or-later. The fetched source retains its original notices and `COPYING.TXT`. Local builds do not copy license files into `bin`; redistribution must include the required notices separately.

This build links SoundTouch statically. Do not redistribute only the DLL and assume that a license notice is sufficient. A compliant binary distribution must also meet the LGPL's source and relinking requirements, including the corresponding library source and suitable application source or relinkable objects/build materials. Keep the generated objects/libraries and source build available, and do not prohibit debugging modifications to the library. This project currently supplies a local build, not a prepared redistributable release package.

## MinHook 1.3.4

Source: https://github.com/TsudaKageyu/minhook

Pinned commit: `c3fcafdc10146beb5919319d0683e44e3c30d537`.

MinHook uses the BSD 2-Clause license, with additional notices for its included disassembler. The fetched source retains those notices. Local builds do not copy them into `bin`; redistribution must include the required notices separately.

## SimpleIni 4.27

Source: https://github.com/brofield/simpleini

Pinned commit: `fd6db69efc40a687bf4ef81486b54066e34992dd`.

SimpleIni is header-only and licensed under MIT. The fetched source retains the copyright and license notice in `SimpleIni.h`. Local builds do not copy license files into `bin`; redistribution must include the required notice separately.

## Spice SDK

Source: https://github.com/spice2x/spice2x.github.io/tree/a48eccab93c9ce11b28d06b3b8ecfe5a018f9def/src/spice2x/sdk

Pinned commit: `a48eccab93c9ce11b28d06b3b8ecfe5a018f9def`.

The official header is vendored unchanged at [external/spice-sdk/spicesdk.h](external/spice-sdk/spicesdk.h). Its SHA-256 is `667b28667301b202f76295eb4e5ffa9937e81fe35100dd2ec9c5e266c511db83`.

The upstream GPLv3 license is retained at [external/spice-sdk/LICENSE](external/spice-sdk/LICENSE); no separate SDK license is supplied upstream. This project's BSD license applies to original code, not the vendored SDK. Redistribution must account for the upstream license terms, not just the project's BSD license. Local builds do not copy license files into `bin`.

No SDK download, Spice2x checkout, or Spice2x build image is required. Spice2x implementation sources are not vendored here. Runtime integration still requires a Spice2x version supporting the SDK.
