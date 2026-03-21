#!/usr/bin/env python3
"""ESP32 WebSocket JPEG receiver.

Usage:
    python tools/ws_receiver.py --out received
"""

import argparse
import asyncio
from datetime import datetime
from pathlib import Path

import websockets


FIXED_HOST = "192.168.1.188"
FIXED_PORT = 80
FIXED_PATH = "/ws"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Receive JPEG frames from ESP32 websocket stream")
    parser.add_argument("--out", default="received", help="Output folder for JPEG files")
    parser.add_argument("--no-save", action="store_true", help="Do not save files, only print receive stats")
    return parser.parse_args()


async def run_client(url: str, out_dir: Path, save_file: bool) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)

    reconnect_delay = 1.0
    frame_count = 0

    while True:
        try:
            async with websockets.connect(url, max_size=8 * 1024 * 1024, ping_interval=20, ping_timeout=20) as ws:
                print(f"connected: {url}")
                await ws.send("latest")

                async for message in ws:
                    if isinstance(message, str):
                        print(f"text: {message}")
                        continue

                    frame_count += 1
                    ts = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
                    size = len(message)

                    if save_file:
                        file_path = out_dir / f"frame_{ts}.jpg"
                        file_path.write_bytes(message)
                        print(f"frame #{frame_count}: {size} bytes -> {file_path}")
                    else:
                        print(f"frame #{frame_count}: {size} bytes")

            reconnect_delay = 1.0
        except Exception as exc:
            print(f"connection lost: {exc}; reconnect in {reconnect_delay:.1f}s")
            await asyncio.sleep(reconnect_delay)
            reconnect_delay = min(reconnect_delay * 2.0, 8.0)


def main() -> None:
    args = parse_args()
    url = f"ws://{FIXED_HOST}:{FIXED_PORT}{FIXED_PATH}"
    out_dir = Path(args.out)

    try:
        asyncio.run(run_client(url, out_dir, not args.no_save))
    except KeyboardInterrupt:
        print("stopped")


if __name__ == "__main__":
    main()
