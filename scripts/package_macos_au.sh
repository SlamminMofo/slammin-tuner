#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-macos-au"
ARTIFACT_DIR="${ROOT_DIR}/artifacts"
COMPONENT="${BUILD_DIR}/src/strobe_tuner/SlamminTunerV1_artefacts/Release/AU/Slammin Tuner.component"
ZIP_PATH="${ARTIFACT_DIR}/Slammin-Tuner-v1.1.6-macOS-AU-universal.zip"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSLAMMIN_TUNER_FORMATS=AU \
  -DSLAMMIN_TUNER_USE_OPENGL=OFF \
  -DSLAMMIN_TUNER_STRICT_WARNINGS=ON

cmake --build "${BUILD_DIR}" --config Release --target SlamminTunerV1_AU --parallel 3

if [[ ! -d "${COMPONENT}" ]]; then
  echo "Expected AU component was not found: ${COMPONENT}" >&2
  exit 1
fi

codesign --force --deep --sign - "${COMPONENT}"
plutil -lint "${COMPONENT}/Contents/Info.plist"
lipo "${COMPONENT}/Contents/MacOS/Slammin Tuner" -verify_arch arm64 x86_64
lipo -info "${COMPONENT}/Contents/MacOS/Slammin Tuner"
xcrun vtool -show-build "${COMPONENT}/Contents/MacOS/Slammin Tuner"

mkdir -p "${HOME}/Library/Audio/Plug-Ins/Components"
rm -rf "${HOME}/Library/Audio/Plug-Ins/Components/Slammin Tuner.component"
cp -R "${COMPONENT}" "${HOME}/Library/Audio/Plug-Ins/Components/"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
auval -v aufx ST26 SLMN

mkdir -p "${ARTIFACT_DIR}"
rm -f "${ZIP_PATH}"
ditto -c -k --sequesterRsrc --keepParent "${COMPONENT}" "${ZIP_PATH}"
echo "Created ${ZIP_PATH}"
