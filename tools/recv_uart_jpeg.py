#!/usr/bin/env python3
import argparse
import binascii
import pathlib
import struct
import time

import serial

MAGIC = b"IMG0"
HEADER_LEN = 12  # magic(4) + len(4) + seq(4)
CRC_LEN = 2


def read_exact(ser: serial.Serial, n: int) -> bytes:
    data = bytearray()
    while len(data) < n:
        chunk = ser.read(n - len(data))
        if not chunk:
            raise TimeoutError("serial read timeout")
        data.extend(chunk)
    return bytes(data)


def sync_magic(ser: serial.Serial) -> None:
    window = bytearray()
    scanned = 0
    sample = bytearray()
    while True:
        b = ser.read(1)
        if not b:
            ascii_sample = bytes(sample).decode("latin1", errors="replace")
            hex_sample = bytes(sample).hex()
            raise TimeoutError(
                f"wait magic timeout (scanned={scanned} bytes, sample_ascii={ascii_sample!r}, sample_hex={hex_sample})"
            )
        window += b
        scanned += 1
        if len(sample) < 64:
            sample += b
        if len(window) > len(MAGIC):
            window = window[-len(MAGIC) :]
        if bytes(window) == MAGIC:
            return


def recv_one_frame(ser: serial.Serial):
    sync_magic(ser)
    hdr = read_exact(ser, 8)
    payload_len, seq = struct.unpack("<II", hdr)

    if payload_len == 0 or payload_len > 2 * 1024 * 1024:
        raise ValueError(f"invalid payload_len={payload_len}")

    payload = read_exact(ser, payload_len)
    crc_recv = struct.unpack("<H", read_exact(ser, CRC_LEN))[0]
    crc_calc = binascii.crc_hqx(payload, 0xFFFF)

    if crc_recv != crc_calc:
        raise ValueError(
            f"crc mismatch: recv=0x{crc_recv:04X}, calc=0x{crc_calc:04X}, seq={seq}"
        )

    if not (payload.startswith(b"\xFF\xD8") and payload.endswith(b"\xFF\xD9")):
        raise ValueError(f"not a complete jpeg frame, seq={seq}")

    return seq, payload


def main():
    parser = argparse.ArgumentParser(description="Receive JPEG from ESP32 UART")
    parser.add_argument("--port", required=True, help="COM port, e.g. COM6")
    parser.add_argument("--baud", type=int, default=2_000_000, help="baud rate")
    parser.add_argument(
        "--count", type=int, default=1, help="number of frames to capture"
    )
    parser.add_argument(
        "--out", default="captures", help="output folder for jpg files"
    )
    parser.add_argument("--timeout", type=float, default=5.0, help="serial timeout(s)")
    parser.add_argument(
        "--boot-wait",
        type=float,
        default=2.5,
        help="wait time after opening COM port for ESP32 boot/camera init",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=5,
        help="max trigger retries per frame when no valid packet is received",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="print debug logs while waiting for frames",
    )
    args = parser.parse_args()

    out_dir = pathlib.Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    with serial.Serial(args.port, args.baud, timeout=args.timeout) as ser:
        # Some ESP32 USB CDC ports auto-reset on open, so wait for firmware readiness.
        try:
            ser.setDTR(False)
            ser.setRTS(False)
        except Exception:
            pass

        if args.debug:
            print(f"[dbg] opened {args.port} @ {args.baud}")
            print(f"[dbg] waiting boot {args.boot_wait:.1f}s")
        time.sleep(args.boot_wait)
        ser.reset_input_buffer()

        for frame_idx in range(args.count):
            last_exc = None
            seq = None
            jpeg = None

            for attempt in range(1, args.retries + 1):
                if args.debug:
                    print(f"[dbg] frame {frame_idx + 1}/{args.count}, trigger attempt {attempt}")

                ser.write(b"C")
                ser.flush()

                try:
                    seq, jpeg = recv_one_frame(ser)
                    break
                except Exception as e:
                    last_exc = e
                    if args.debug:
                        waiting = ser.in_waiting
                        print(f"[dbg] recv failed: {e}; in_waiting={waiting}")
                    # Re-sync on next try.
                    ser.reset_input_buffer()

            if jpeg is None or seq is None:
                raise RuntimeError(f"failed to receive frame {frame_idx + 1}: {last_exc}")

            ts = time.strftime("%Y%m%d_%H%M%S")
            path = out_dir / f"frame_{seq:06d}_{ts}.jpg"
            path.write_bytes(jpeg)
            print(f"saved: {path} ({len(jpeg)} bytes)")


if __name__ == "__main__":
    main()
