#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Independent reference for the YOLOX person pipeline (numpy + onnxruntime + OpenCV).

Re-implements the official YOLOX ONNX demo pre/post-processing (preproc, demo_postprocess,
multiclass NMS reduced to the person class) so the C++ implementation can be cross-checked:

  python3 scripts/yolox_reference.py MODEL.onnx IMAGE [--compare CPP_OUTPUT.txt]

CPP_OUTPUT.txt is the stdout of `detect_image MODEL IMAGE` ("x y w h conf" per line).
Exit 0 if every reference box has a C++ box with IoU >= 0.9 and |conf diff| <= 0.02 and counts
match; exit 1 otherwise. Requires: numpy, onnxruntime, opencv-python(-headless).
"""
import argparse
import sys

import cv2
import numpy as np
import onnxruntime as ort

SCORE_THRESH = 0.35
NMS_IOU = 0.45


def preproc(img, size):
    padded = np.ones((size[0], size[1], 3), dtype=np.uint8) * 114
    r = min(size[0] / img.shape[0], size[1] / img.shape[1])
    resized = cv2.resize(
        img, (int(img.shape[1] * r), int(img.shape[0] * r)), interpolation=cv2.INTER_LINEAR
    ).astype(np.uint8)
    padded[: resized.shape[0], : resized.shape[1]] = resized
    chw = padded.transpose((2, 0, 1))
    return np.ascontiguousarray(chw, dtype=np.float32), r


def demo_postprocess(outputs, img_size):
    grids, strides = [], []
    for stride in (8, 16, 32):
        hsize, wsize = img_size[0] // stride, img_size[1] // stride
        xv, yv = np.meshgrid(np.arange(wsize), np.arange(hsize))
        grids.append(np.stack((xv, yv), 2).reshape(1, -1, 2))
        strides.append(np.full((*grids[-1].shape[:2], 1), stride))
    grids = np.concatenate(grids, 1)
    strides = np.concatenate(strides, 1)
    outputs[..., :2] = (outputs[..., :2] + grids) * strides
    outputs[..., 2:4] = np.exp(outputs[..., 2:4]) * strides
    return outputs


def nms(boxes, scores, thr):
    x1, y1, x2, y2 = boxes.T
    areas = (x2 - x1) * (y2 - y1)
    order = scores.argsort()[::-1]
    keep = []
    while order.size > 0:
        i = order[0]
        keep.append(i)
        xx1 = np.maximum(x1[i], x1[order[1:]])
        yy1 = np.maximum(y1[i], y1[order[1:]])
        xx2 = np.minimum(x2[i], x2[order[1:]])
        yy2 = np.minimum(y2[i], y2[order[1:]])
        inter = np.maximum(0.0, xx2 - xx1) * np.maximum(0.0, yy2 - yy1)
        iou = inter / (areas[i] + areas[order[1:]] - inter)
        order = order[1:][iou <= thr]
    return keep


def detect(model, image):
    sess = ort.InferenceSession(model, providers=["CPUExecutionProvider"])
    inp = sess.get_inputs()[0]
    size = (inp.shape[2], inp.shape[3])
    blob, ratio = preproc(image, size)
    out = sess.run(None, {inp.name: blob[None]})[0]
    pred = demo_postprocess(out, size)[0]
    scores = pred[:, 4] * pred[:, 5]  # objectness * person (class 0)
    mask = scores >= SCORE_THRESH
    if not mask.any():
        return []
    boxes = pred[mask, :4]
    xyxy = np.stack(
        [boxes[:, 0] - boxes[:, 2] / 2, boxes[:, 1] - boxes[:, 3] / 2,
         boxes[:, 0] + boxes[:, 2] / 2, boxes[:, 1] + boxes[:, 3] / 2], 1) / ratio
    sc = scores[mask]
    keep = nms(xyxy, sc, NMS_IOU)
    h, w = image.shape[:2]
    res = []
    for k in keep:
        x1, y1, x2, y2 = np.clip(xyxy[k], [0, 0, 0, 0], [w, h, w, h])
        res.append((x1, y1, x2 - x1, y2 - y1, float(sc[k])))
    return res


def iou_xywh(a, b):
    ax2, ay2, bx2, by2 = a[0] + a[2], a[1] + a[3], b[0] + b[2], b[1] + b[3]
    iw = max(0.0, min(ax2, bx2) - max(a[0], b[0]))
    ih = max(0.0, min(ay2, by2) - max(a[1], b[1]))
    inter = iw * ih
    return inter / (a[2] * a[3] + b[2] * b[3] - inter)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("model")
    ap.add_argument("image")
    ap.add_argument("--compare")
    args = ap.parse_args()
    img = cv2.imread(args.image)
    ref = detect(args.model, img)
    for r in ref:
        print("%d %d %d %d %.4f" % (round(r[0]), round(r[1]), round(r[2]), round(r[3]), r[4]))
    if not args.compare:
        return 0
    cpp = [tuple(float(v) for v in line.split()) for line in open(args.compare) if line.strip()]
    ok = len(cpp) == len(ref)
    print(f"reference={len(ref)} cpp={len(cpp)}", file=sys.stderr)
    for r in ref:
        best = max(cpp, key=lambda c: iou_xywh(r, c), default=None)
        if best is None:
            ok = False
            continue
        i, dc = iou_xywh(r, best), abs(r[4] - best[4])
        print(f"ref box IoU={i:.3f} dconf={dc:.4f}", file=sys.stderr)
        ok &= (i >= 0.9) and (dc <= 0.02)
    print("MATCH" if ok else "MISMATCH", file=sys.stderr)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
