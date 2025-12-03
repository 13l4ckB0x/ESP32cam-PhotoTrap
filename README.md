# ESP32-CAM-Telegram-UXGA-Photo-Trigger
Stable 1600×1200 capture → Telegram without reboots
📋 Result Summary
Resolution: 1600×1200 (UXGA, 2 MP)
File size: 70-85 kB (jpeg_quality = 10) – good trade-off for CNN training
No crashes: stack-overflow fixed by lower XCLK & quality, single frame buffer
Delivery: photo sent to Telegram bot in 1-2 s
Free RAM: > 180 kB heap, > 1.7 MB PSRAM – headroom for future features
Button trigger: GPIO 13, debounced, LED flash feedback
Suitable for collecting a starter dataset for YOLO defect-detection on polished metal cylinders (≤ 80 mm Ø).
For smaller defects or larger parts upgrade to 5 MP OV5640 or Raspberry Pi HQ camera.
