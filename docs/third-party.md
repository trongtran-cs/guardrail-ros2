# Third-party components

| Component | Use | Licence | Notes |
|---|---|---|---|
| ROS 2 Jazzy | middleware | Apache-2.0 | |
| OpenCV | HOG detector, image handling | Apache-2.0 | |
| ONNX Runtime (`ros-jazzy-onnxruntime-vendor`, 1.24.3) | YOLOX inference | MIT | |
| YOLOX ONNX weights (`yolox_nano`, `yolox_tiny`, release 0.1.1rc0) | person detection | Apache-2.0 (Megvii-BaseDetection/YOLOX) | Downloaded by `scripts/fetch_model.sh`, SHA-256 pinned (trust on first use); trained on COCO (annotations CC BY 4.0) |
| OpenCV sample video frame (`pedestrians.jpg`) | test fixture | Apache-2.0 | see `src/guardrail_detector_node/test/fixtures/NOTICE.md` |
| ONNX Runtime Web (`onnxruntime-web@1.17.3`, loaded from jsDelivr by `ui/index.html`) | in-browser YOLOX inference | MIT | runtime CDN dependency of the demo page only |
| `ui/models/yolox_nano.onnx`, `ui/samples/*` | web demo inputs | Apache-2.0 | committed copies; see `ui/samples/NOTICE.md` |
| googletest | unit tests | BSD-3-Clause | fetched or system package |

Ultralytics YOLO (AGPL-3.0) was deliberately **not** used: its licence conflicts with this
repository's Apache-2.0 licence.
