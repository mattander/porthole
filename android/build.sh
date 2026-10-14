#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${BUILD_DIR:-$project_root/local/android-preview-build}"
: "${ANDROID_API_JAR:?Set ANDROID_API_JAR to the verified Android API compilation jar}"
: "${R8_JAR:?Set R8_JAR to the verified Google R8 compiler jar}"
python3 - "$ANDROID_API_JAR" "$R8_JAR" <<'CHECKSUMS'
import hashlib, pathlib, sys
expected = [
    '84072541cbb711eff89f7277100ff854929a446dba7ceb1b195c340e0b4fd3cb',
    '59753e70a74f918389cc87f1b7d66b5c0862932559167425708ded159e3de439',
]
for name, digest in zip(sys.argv[1:], expected):
    if hashlib.sha256(pathlib.Path(name).read_bytes()).hexdigest() != digest:
        raise SystemExit('Compilation dependency checksum mismatch: ' + name)
CHECKSUMS
mkdir -p "$build_dir/classes" "$build_dir/dex"
if [[ ! -f "$build_dir/preview.keystore" ]]; then
    keytool -genkeypair -keystore "$build_dir/preview.keystore" -storepass android \
        -keypass android -alias preview -keyalg RSA -keysize 2048 -validity 3650 \
        -dname 'CN=Porthole Local Preview' >/dev/null 2>&1
    chmod 600 "$build_dir/preview.keystore"
fi
javac --release 8 -classpath "$ANDROID_API_JAR" -d "$build_dir/classes" \
    "$project_root/android/src/org/porthole/preview/PreviewActivity.java"
jar cf "$build_dir/classes.jar" -C "$build_dir/classes" .
java -cp "$R8_JAR" com.android.tools.r8.D8 --min-api 16 \
    --lib "$ANDROID_API_JAR" --output "$build_dir/dex" "$build_dir/classes.jar"
aapt package -f -M "$project_root/android/AndroidManifest.xml" \
    -I "$ANDROID_API_JAR" -A "$project_root/web" -F "$build_dir/unsigned.apk"
(cd "$build_dir/dex" && zip -q "$build_dir/unsigned.apk" classes.dex)
zipalign -f 4 "$build_dir/unsigned.apk" "$build_dir/aligned.apk"
apksigner sign --ks "$build_dir/preview.keystore" --ks-key-alias preview \
    --ks-pass pass:android --key-pass pass:android \
    --out "$build_dir/porthole-preview.apk" "$build_dir/aligned.apk"
apksigner verify --verbose "$build_dir/porthole-preview.apk"
aapt dump badging "$build_dir/porthole-preview.apk"
