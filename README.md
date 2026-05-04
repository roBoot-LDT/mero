# mero

A 45-day discipline project: build a remotely-controlled tracked/wheeled robot that finds 3 QR codes in camera view less than 2 minutes on uneven terrain

**Author:** Maksim Nemkov
**Started:** 2026-05-04
**Hard deadline:** 2026-06-17 (45 days)

---

## Why this project exists

This repository is not primarily a robotics project. It is a discipline experiment.

The goal is to complete one self-initiated technical project from start to finish, on time, with a publicly visible result. The robot is the medium, not the message. The message is: "I can finish what I start"

For three years prior to this project, no voluntary technical project was completed. This is the attempt to break that pattern. The project is intentionally constrained in scope so that the binding constraint is follow-through, not technical capability.

---

## What the robot does (Definition of Done)

A wheeled robot that:

1. Is controlled remotely over BLE from a custom 5-button pad (4 directions + start/stop).
2. Carries an ESP32-CAM that streams video over WiFi to a server.
3. The server detects QR codes in the video stream using OpenCV in real time.
4. The operator drives the robot on uneven terrain so that a pre-placed 3 QR codes must be founded and be inside the camera frame less than 2 minutes.

**The project is considered done when:**

- 5 out of 10 attempts succeed at founding the 3 QRs in frame less than 2 minutes on a defined uneven surface.
- All 10 attempts are recorded on video and published in this repository.
- A final write-up exists in `docs/postmortem.md` covering what worked, what didn't, what the next iteration would change.

**Not in scope for v1 (these belong to v2 if there ever is one):**

- Camera gimbal with servo stabilization
- Autonomous QR search
- Autonomous driving
- Obstacle avoidance
- Battery telemetry, OTA updates, app GUI

If during the 45 days the temptation arises to add any of the above, the rule is: **note it in `IDEAS.md` and do not implement it.**

---

## Architecture

Three independent nodes, intentionally separated to avoid one ESP32 trying to do everything.

```
+---------------------+        BLE 5         +-----------------------+
|  REMOTE             | <==================> |  ROBOT MAIN           |
|  ESP32 dev board    |                      |  ESP32-S3-N16R8       |
|  5 tactile buttons  |                      |  motor driver         |
|                     |                      |  (TB6612FNG / DRV8833)|
+---------------------+                      |  2x DC motors         |
                                             |  2x 18650 + BMS 2S    |
                                             |  buck 5V              |
                                             +-----------+-----------+
                                                         |
                                                         | WiFi (same AP)
                                                         |
                                             +-----------v-----------+
                                             |  ESP32-CAM            |
                                             |  MJPEG stream / HTTP  |
                                             +-----------+-----------+
                                                         |
                                                         | WiFi
                                                         |
                                             +-----------v-----------+
                                             |  SERVER (laptop or VPS)|
                                             |  Python + OpenCV       |
                                             |  pyzbar / cv2.QRCode   |
                                             |  publishes detection   |
                                             |  events                |
                                             +------------------------+
```

**Why three nodes and not one:** a single ESP32-CAM cannot reliably run BLE + WiFi + camera + motor PWM at the same time. It has too few free GPIOs after the camera takes its pins, and BLE/WiFi share the radio. Splitting responsibilities makes each node simpler and debuggable in isolation.

**Why the server and not on-device QR detection:** ESP32-CAM can do QR detection locally (`esp32-camera` + `quirc` works), but routing detection to a Python server gives a cleaner debugging surface, easier logging, and faster iteration on detection parameters during the 45 days. After the project is done, on-device detection is a candidate for v2.

---

## Hardware

| Component | Notes |
|---|---|
| ESP32-S3-N16R8 dev board | Robot main controller. BLE 5 + WiFi 4. |
| ESP32 (any DevKit) | Remote control unit. |
| ESP32-CAM | Video streaming over WiFi. |
| TB6612FNG or DRV8833 | Motor driver. **Not L293D** — it cannot handle the stall current of these motors. |
| 2x DC motors (RS-385 class, 5V nominal) | Drive motors. |
| 2x 18650 Li-Ion cells + 2S BMS | Robot power. ~7.4V nominal. |
| MP1584 or LM2596 buck converter | Steps 7.4V down to 5V for ESP32 / driver logic. |
| 5x tactile buttons + pull-ups | Remote keypad. |
| Wheels (printed) | Diameter ≥ 80 mm, otherwise robot is geared too fast. |
| 3D printer (Bambu Lab P1S) | Chassis, mounts, wheels. |
| Multimeter, soldering iron, lab PSU | Already on hand. |

**Critical caveats discovered before start:**

- ESP32-S3 supports BLE 5 only, **not** classic Bluetooth. The remote must speak BLE GATT.
- L293D is too weak for these motors and will burn out under stall load. Replaced with TB6612FNG / DRV8833.
- Bare DC motors without gearing rev too fast (5–15k RPM) and have low torque at low speed — large wheels or geared replacements are required for terrain.
- Two 18650 in series is 7.4V. Connecting that directly to a 3.3V or 5V logic input destroys the board. A buck converter is mandatory.

---

Long-form notes live in Obsidian. What lands in `docs/devlog.md` is the public summary, written at least once per week.

---

## Milestones

Three checkpoints. Each has a measurable, video-verifiable state. Missing a checkpoint by more than 3 days triggers a written re-plan in `docs/milestones.md`, not a silent slip.

### M1 — 2026-05-18 (day 15): bench integration

- Chassis printed and assembled (motors mounted, wheels on, can be held in hand).
- ESP32-S3 drives both motors forward / back / left / right via TB6612FNG over USB-serial commands. No BLE yet.
- ESP32-CAM streams MJPEG to laptop over WiFi.
- Python server pulls stream, runs `cv2.QRCodeDetector` (or `pyzbar`), logs `qr_seen=True/False` per frame.
- Video evidence: 30 seconds showing motors responding, stream live, QR detection logged.

### M2 — 2026-06-01 (day 30): wireless robot

- BLE remote control works. Robot drives untethered.
- Robot runs from 18650 + BMS + buck. No bench PSU.
- Video stream survives motion. QR detection still works on a moving feed.
- Robot drives reliably on flat indoor surface.
- Video evidence: 2 minutes of free driving, QR detected on the move.

### M3 — 2026-06-17 (day 45): final challenge

- Defined uneven test surface (photographed, dimensioned).
- 10 attempts at the 2-minute QR-hold task. All recorded.
- Postmortem written.
- README updated with results — pass or fail honestly.

If M3 succeeds: project is done.
If M3 fails: the project is **still done.** A failed-but-finished project beats an abandoned one. A failed run still gets written up and committed.

## License

MIT. See `LICENSE`.

---

## Contact

This is a solo project. Issues and PRs are not expected, but the repository is public so that finishing the project is a publicly visible commitment.
