// SPDX-License-Identifier: Apache-2.0
// Browser-side YOLOX person pipeline: letterbox -> (ONNX Runtime Web) -> decode -> NMS ->
// distance -> supervisor inputs. Pure functions, no DOM, so it can be cross-checked in Node
// against scripts/yolox_reference.py. Mirrors yolox_postprocess.cpp and guardrail_core geometry.
(function (root) {
  "use strict";
  const STRIDES = [8, 16, 32];
  const COLS = 85; // 4 box + 1 objectness + 80 COCO classes; class 0 = person

  const sizeOk = (w, h) => w > 0 && h > 0 && w % 32 === 0 && h % 32 === 0;

  function expectedRows(w, h) {
    if (!sizeOk(w, h)) return 0;
    return STRIDES.reduce((n, s) => n + (w / s) * (h / s), 0);
  }

  /** Letterbox geometry: scale r, top-left aligned, pad to size x size (YOLOX demo preproc). */
  function letterbox(srcW, srcH, size) {
    const r = Math.min(size / srcH, size / srcW);
    return { r, nw: Math.floor(srcW * r), nh: Math.floor(srcH * r) };
  }

  /** RGBA bytes (size x size, already letterboxed) -> CHW float32 in BGR order, 0..255. */
  function blobFromRGBA(rgba, w, h) {
    const plane = w * h, out = new Float32Array(3 * plane);
    for (let i = 0, p = 0; i < plane; i++, p += 4) {
      out[i] = rgba[p + 2];            // B
      out[plane + i] = rgba[p + 1];    // G
      out[2 * plane + i] = rgba[p];    // R
    }
    return out;
  }

  /** Decode raw (non-decoded) YOLOX output [rows x 85] into person boxes (input-pixel coords). */
  function decodePersons(data, rows, w, h, scoreThresh) {
    if (!data || rows === 0 || rows !== expectedRows(w, h)) return { ok: false, boxes: [] };
    const boxes = [];
    let row = 0;
    for (const stride of STRIDES) {
      const gw = w / stride, gh = h / stride;
      for (let gy = 0; gy < gh; gy++) {
        for (let gx = 0; gx < gw; gx++, row++) {
          const o = row * COLS;
          const score = data[o + 4] * data[o + 5];
          if (!(score >= scoreThresh)) continue; // also rejects NaN
          const cx = (data[o] + gx) * stride, cy = (data[o + 1] + gy) * stride;
          const bw = Math.exp(data[o + 2]) * stride, bh = Math.exp(data[o + 3]) * stride;
          if (![cx, cy, bw, bh].every(Number.isFinite) || bw <= 0 || bh <= 0) continue;
          boxes.push({ x1: cx - bw / 2, y1: cy - bh / 2, x2: cx + bw / 2, y2: cy + bh / 2, score });
        }
      }
    }
    return { ok: true, boxes };
  }

  function iou(a, b) {
    const iw = Math.max(0, Math.min(a.x2, b.x2) - Math.max(a.x1, b.x1));
    const ih = Math.max(0, Math.min(a.y2, b.y2) - Math.max(a.y1, b.y1));
    const inter = iw * ih;
    const ar = (q) => Math.max(0, q.x2 - q.x1) * Math.max(0, q.y2 - q.y1);
    const uni = ar(a) + ar(b) - inter;
    return uni > 0 ? inter / uni : 0;
  }

  function nms(boxes, iouThresh) {
    const sorted = boxes.slice().sort((a, b) => b.score - a.score);
    const kept = [];
    for (const c of sorted) if (!kept.some((k) => iou(c, k) > iouThresh)) kept.push(c);
    return kept;
  }

  /** Pinhole d = f*H/h (guardrail::estimateDistance). */
  function estimateDistance(hPx, focalPx, personH) {
    if (![hPx, focalPx, personH].every((v) => Number.isFinite(v) && v > 0)) return null;
    const m = (focalPx * personH) / hPx;
    return Number.isFinite(m) ? m : null;
  }

  /** guardrail::summarise: nearest person distance + its confidence; invalid => conf 0. */
  function summarise(dets, focalPx, personH) {
    let nearest = -1, conf = 1, invalid = 0, have = false, idx = -1;
    dets.forEach((d, i) => {
      const dist = estimateDistance(d.h, focalPx, personH);
      const ok = Number.isFinite(d.conf) && d.conf >= 0 && d.conf <= 1;
      if (dist === null || !ok) { invalid++; return; }
      if (!have || dist < nearest) { have = true; nearest = dist; conf = d.conf; idx = i; }
    });
    if (invalid) conf = 0;
    return { nearest, conf, invalid, idx };
  }

  const api = { expectedRows, letterbox, blobFromRGBA, decodePersons, iou, nms, estimateDistance, summarise };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.YOLOX = api;
})(typeof self !== "undefined" ? self : this);
