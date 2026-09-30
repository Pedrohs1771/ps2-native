# Android runner

## Building a game package

Run the desktop builder from the repository root:

```sh
python3 -m tools.ps2native build --iso /path/to/game.iso --target android --out /path/to/new-package
```

The host needs JDK 17, Gradle 8.7 or newer, Android SDK platform 34, NDK
`28.2.13676358`, and Android CMake `3.22.1`. Set `ANDROID_SDK_ROOT` or
`ANDROID_HOME`; pass `--gradle /path/to/gradle` when Gradle is not on `PATH`.
The builder also checks a cached Gradle wrapper distribution under
`~/.gradle/wrapper/dists`. This checkout has `gradle-wrapper.properties` but no
wrapper scripts or wrapper JAR, so the builder does not rely on `./gradlew`.

The builder creates a separate project under the per-ISO workspace, copies the
extracted disc to `app/src/main/assets/game/`, and passes generated C++ through
`PS2X_GENERATED_CODE_DIR`. It does not write title files into the checked-in
Android project. Each APK gets an application ID derived from the ISO SHA-256
prefix and a stable output name such as `game-<hash>-arm64.apk`.

If Gradle, JDK, SDK, NDK, CMake, or `Ps2PackageActivity` is missing, the command
records the exact blocker and staged project path in `manifest.json`, exits as
`blocked`, and does not report an APK artifact. The builder does not install
SDK components or accept Android SDK licenses on the host's behalf.

## Runtime asset path

`Ps2PackageActivity` copies `assets/game/` into
`getFilesDir()/game/` before starting the native library. The runtime resolves
the boot ELF at `ANativeActivity::internalDataPath/game/boot.elf`; no ISO path
or external-storage permission is needed. The APK embeds the complete extracted
disc tree, so large games produce large APKs. Split/OBB delivery is not
implemented.

The checked-in Gradle project can build a generic runtime with the placeholder
registration source when `PS2X_GENERATED_CODE_DIR` is empty. The host builder is
the supported route for a game-specific generated build. Runtime output is
available through `adb logcat -s ps2x`.
