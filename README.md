# Mercury Chat

Mercury Chat is a small Qt/C++ client for the Rhizomatica Mercury HF modem. It keeps the modem as a separate process and talks to Mercury through its stable VARA-compatible TCP TNC interface.

## Mercury Integration Notes

Mercury v2 is a C HF modem using FreeDV waveforms. Its public host interface is:

- ARQ control TCP port: `8300` by default. Commands are ASCII lines ending in `\r`.
- ARQ data TCP port: `8301` by default. Payload is a raw reliable byte stream while connected.
- Broadcast TCP port: `8100` by default. Broadcast uses KISS framing and is independent from ARQ.
- CAT/PTT can be handled by Mercury itself with `-R <hamlib_model> -A <device>` or by this client through hamlib.

Useful Mercury commands used here:

- `MYCALL <call>` sets the local callsign.
- `BW2300`, `BW500`, or `BW2750` sets the ARQ bandwidth token.
- `CHAT ON` and `LISTEN ON` prepare the modem for incoming calls.
- `CQFRAME <call> <bw>` transmits a compact DATAC13 beacon.
- `CONNECT <mycall> <theircall>` starts an ARQ connection.
- `DISCONNECT` ends the current ARQ session.

Received `CQFRAME <call> <bw>` lines are shown in the beacon table. Double-click a beacon callsign to send `CONNECT <mycall> <theircall>`.

## Chat Payload Format

Chat messages are UTF-8 JSON Lines on Mercury's ARQ data port:

```json
{"v":1,"type":"msg","from":"BV1AAA","time":"2026-05-16T02:00:00.000Z","text":"你好"}
```

The newline is only the message delimiter; newlines inside text are JSON-escaped. This keeps CJK text as UTF-8 and still allows future fields such as delivery receipts.

## Build

Requirements:

- CMake 3.21+
- Qt 6 Core, Network, Widgets
- hamlib development package

```sh
cmake -S mercury-chat -B mercury-chat/build
cmake --build mercury-chat/build
ctest --test-dir mercury-chat/build --output-on-failure
```

If you need a no-CAT build:

```sh
cmake -S mercury-chat -B mercury-chat/build -DMERCURYCHAT_USE_HAMLIB=OFF
cmake --build mercury-chat/build
```

## Run

Start Mercury first, for example:

```sh
./mercury -p 8300 -b 8100 -R <hamlib_model_id> -A <radio_device>
```

If Mercury is already controlling PTT through hamlib, use this client's CAT panel mainly for readback and cautious manual tests. Avoid keying PTT from two programs at the same time.

Then launch:

```sh
./mercury-chat/build/mercury-chat
```

Set your callsign, connect to `127.0.0.1:8300`, initialize the station, and send or wait for a beacon.
