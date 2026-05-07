import cv2
import requests
import numpy as np
import time
import logging

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s %(levelname)s %(message)s',
    handlers=[
        logging.FileHandler("qr_log.txt"),
        logging.StreamHandler()
    ]
)
log = logging.getLogger("mero")

STREAM_URL = "http://192.168.3.49/stream"

def main():
    log.info(f"Connecting to {STREAM_URL}")
    detector = cv2.QRCodeDetector()
    qr_visible_since = None
    qr_held_seconds = 0

    try:
        r = requests.get(
        STREAM_URL,
        stream=True,
        timeout=(10, 60),  # (connect timeout, read timeout)
        proxies={"http": None, "https": None},
        headers={"Connection": "keep-alive"}
    )
        log.info(f"Connected, status: {r.status_code}")

        buf = b""

        for chunk in r.iter_content(chunk_size=4096):
            buf += chunk
            start = buf.find(b'\xff\xd8')
            end = buf.find(b'\xff\xd9')

            if start == -1 or end == -1 or end <= start:
                # Не накопили полный кадр ещё
                if len(buf) > 100000:
                    buf = buf[-50000:]  # не даём буферу расти бесконечно
                continue

            jpg = buf[start:end+2]
            buf = buf[end+2:]

            img = cv2.imdecode(np.frombuffer(jpg, np.uint8), cv2.IMREAD_COLOR)
            if img is None:
                continue

            data, _, _ = detector.detectAndDecode(img)

            if data:
                now = time.time()
                if qr_visible_since is None:
                    qr_visible_since = now
                    log.info(f"QR detected: '{data}' — timer started")
                qr_held_seconds = now - qr_visible_since
                log.info(f"QR held: {qr_held_seconds:.1f}s")
                cv2.putText(img, f"QR: {qr_held_seconds:.1f}s",
                    (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                if qr_held_seconds >= 120:
                    log.info("SUCCESS: QR held for 2 minutes!")
            else:
                if qr_visible_since is not None:
                    log.info(f"QR lost after {qr_held_seconds:.1f}s")
                qr_visible_since = None
                qr_held_seconds = 0

            cv2.imshow("mero stream", img)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

    except KeyboardInterrupt:
        log.info("Stopped by user")
    except Exception as e:
        log.error(f"Error: {e}")
    finally:
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()