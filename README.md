| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |

# BLEMIDI program build with NimBLE GATT Server (DEPRECATED)
由于NimBLE的C++版本（h2zero/esp-nimble-cpp）更方便且更适合本项目要求，本项目迁移至esp32_blemidi_cpp。此地址不再更新

## Overview

本项目旨在设计一种MIDI乐器，它可以通过蓝牙连接到手机或其他设备，实现MIDI控制。音乐的渲染由客户端设备上的其他应用程序完成，如iOS上的“库乐队”。
BLE MIDI功能的实现使用了ESP32和NimBLE库。它可以作为一个GATT服务器，允许BLE MIDI客户端连接并发送MIDI消息。

## 接口
blemidi.h中定义的接口函数主要有两个：
```c
void send_midi_note_on(uint8_t note, uint8_t velocity);
void send_midi_note_off(uint8_t note);
```
这两个函数用于发送MIDI音符开和关的消息。

