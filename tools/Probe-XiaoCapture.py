import argparse
import time

import serial


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Reset a XIAO, request one capture, and print diagnostics."
    )
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--seconds", type=float, default=55.0)
    parser.add_argument("--boot-wait", type=float, default=4.0)
    parser.add_argument(
        "--skip-capture",
        action="store_true",
        help="Reset and read diagnostics without sending the capture command.",
    )
    args = parser.parse_args()

    with serial.Serial(args.port, 115200, timeout=0.2) as device:
        device.dtr = False
        device.rts = True
        time.sleep(0.15)
        device.rts = False
        started = time.monotonic()
        capture_sent = args.skip_capture
        while time.monotonic() - started < args.seconds:
            if not capture_sent and time.monotonic() - started >= args.boot_wait:
                print("[probe] sending c", flush=True)
                device.write(b"c")
                device.flush()
                capture_sent = True
            data = device.readline()
            if data:
                print(data.decode("utf-8", errors="replace").rstrip(), flush=True)


if __name__ == "__main__":
    main()
