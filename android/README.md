# Offline frame preview

`org.porthole.preview` (the generic package identifier) is a removable, fullscreen preview of the original `web/` frontend. It bundles the HTML, ES5 JavaScript, styles, and SVG icons and opens `file:///android_asset/index.html?demo=1`. The large clock, month calendar, right-hand agenda, and bottom weather strip were verified on the physical PF1007L at 1280×800 using its installed WebView 44.0.2403.119.

The calendar, clock, and weather are explicitly labeled samples. Apple/iCloud and live weather are not connected. The preview requests **no Android permissions**, blocks network loads and external navigation, has no native JavaScript bridge, disables browser storage and cookies, and contains no credentials. Packaged assets remain accessible when ordinary file access is disabled, as documented by [Android WebSettings](https://developer.android.com/reference/android/webkit/WebSettings#setAllowFileAccess(boolean)).

It runs as an ordinary app, keeps the screen awake while visible, and requests landscape fullscreen. It does not become the default Home app or start at boot. Long-press anywhere to **Reload sample** or **Exit preview**. Exit was verified to return to the original AiMOR app. The frame remains on its original Android 6.0.1 and Linux 3.4.39.

## Build

Requires Java 17 or newer, Python 3, `aapt`, `apksigner`, `zipalign`, and `zip`. These were installed only in the project's existing isolated Ubuntu build container; no complete Android SDK or Gradle installation was needed. The Android API 16 compilation stubs keep the Java source limited to older APIs. The manifest targets API 23, matching this frame. API 19's immersive-sticky flag is represented by its documented numeric value; earlier devices are not validated targets.

Download these compilation dependencies over HTTPS into an ignored local directory:

- [Google Android compilation jar 4.1.1.4](https://repo.maven.apache.org/maven2/com/google/android/android/4.1.1.4/android-4.1.1.4.jar), SHA-256 `84072541cbb711eff89f7277100ff854929a446dba7ceb1b195c340e0b4fd3cb`.
- [Google R8 8.3.37](https://dl.google.com/dl/android/maven2/com/android/tools/r8/8.3.37/r8-8.3.37.jar), SHA-256 `59753e70a74f918389cc87f1b7d66b5c0862932559167425708ded159e3de439`.

Published repository SHA-1 checksums were independently matched before first use; `build.sh` checks the above SHA-256 pins before compiling. Build with:

```sh
ANDROID_API_JAR=/absolute/path/android.jar R8_JAR=/absolute/path/r8.jar bash android/build.sh
```

The default output and generated development keystore stay in ignored `local/android-preview-build/`. The development signing password is `android`; this key is for this local preview, not production distribution. APK signature verification and manifest inspection are part of the build. The current local APK is signed with v1, v2, and v3 signatures; Android 6 uses v1.

Only install after confirming the target frame and authorization. Select that transport explicitly:

```sh
adb -s "$FRAME_SERIAL" install --no-streaming /absolute/path/porthole-preview.apk
adb -s "$FRAME_SERIAL" shell am start -W -n org.porthole.preview/.PreviewActivity
```

Remove the preview with `adb -s "$FRAME_SERIAL" uninstall org.porthole.preview`. This is an app removal, not a firmware restore.

The public source now uses the generic `org.porthole.preview` package. The historical device checks below used the earlier preview build. Rebuilding with the new identifier creates a separate app; it does not replace an existing preview installation automatically.

## Device validation — 2026-10-13

Installation and launch succeeded. The WebView reported viewport and document dimensions of exactly 1280×800 with 35 day buttons in October 2026. Native app diagnostics reported no JavaScript errors or crash. Screenshots verified the day-details view, next month, Today, and a six-week May 2027 layout. Android input injection exercised these controls; the user's physical touch-panel check is still separate. The long-press menu opened and Exit returned to AiMOR; the preview was relaunched afterward. Wi-Fi remained disabled.

[Fictional native dashboard preview](../docs/images/porthole-preview-1280x800.png). Original screenshots, compilation artifacts, and private installation evidence remain in ignored `local/`.

The next implementation step is a host-side calendar/weather backend and a separately reviewed online-capable display mode. The existing frontend contract is documented in [web/README.md](../web/README.md). A sample preview does not establish iCloud integration, live weather, reboot persistence, or replacement-OS compatibility.
