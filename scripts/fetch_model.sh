#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Download a YOLOX ONNX model (Megvii-BaseDetection/YOLOX, Apache-2.0; COCO-trained) and verify
# its SHA-256. The hashes below were recorded by the maintainer on 2026-11-22 from the
# 0.1.1rc0 release assets (trust-on-first-use): re-verify against the upstream release if you
# need stronger provenance.
#
#   ./scripts/fetch_model.sh [yolox_nano|yolox_tiny] [dest_dir]
set -euo pipefail

NAME="${1:-yolox_nano}"
DEST="${2:-models}"
BASE="https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0"

case "$NAME" in
  yolox_nano) SHA256="c789161ed43c8269fcd4e67c67eeeb4e80c622da2eb296a20bc6007bd18a0b7d" ;;
  yolox_tiny) SHA256="427cc366d34e27ff7a03e2899b5e3671425c262ea2291f88bb942bc1cc70b0f7" ;;
  *) echo "unknown model '$NAME' (yolox_nano|yolox_tiny)" >&2; exit 2 ;;
esac

mkdir -p "$DEST"
OUT="$DEST/$NAME.onnx"
if [ ! -f "$OUT" ]; then
  curl -fsSL -o "$OUT.part" "$BASE/$NAME.onnx"
  mv "$OUT.part" "$OUT"
fi
if command -v sha256sum >/dev/null; then ACTUAL="$(sha256sum "$OUT" | cut -d' ' -f1)"; else ACTUAL="$(shasum -a 256 "$OUT" | cut -d' ' -f1)"; fi
if [ "$ACTUAL" != "$SHA256" ]; then
  echo "SHA-256 mismatch for $OUT (got $ACTUAL)" >&2
  rm -f "$OUT"
  exit 1
fi
echo "ok: $OUT"
