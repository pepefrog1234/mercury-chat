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

Mercury Chat deliberately leaves Mercury's adaptive FreeDV mode selection alone. The GUI only sends the selected `BW...` token and advertises that token in beacons; it does not select DATAC modes, force speed levels, or override ARQ gear shifting. `BITRATE`, `SN`, `BUFFER`, and `PTT` lines from Mercury are treated as status telemetry.

The main `Chat` tab is kept for chat text plus beacon and link activity. Mercury process output, TNC control lines, CAT messages, and telemetry are shown in the separate `Modem Status` tab.

Operator settings are saved with Qt's platform settings store under `Rhizomatica/MercuryChat`. Saved fields include callsign, TNC host/ports, modem path/arguments, audio input/output device, sound system, RX channel, beacon interval, selected bandwidth, CAT radio, serial port, baud rate, RTS/DTR states, PTT method, and window geometry.

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

By default, Mercury Chat tries to start the Mercury modem automatically when the GUI opens. It looks for the executable in this order:

- `MERCURY_MODEM_PATH`
- next to the `mercury-chat` executable
- common developer layouts such as `../../mercury/mercury`
- `mercury` in `PATH`

When using the sibling Mercury source checkout, build Mercury first so that `../../mercury/mercury` exists. On platforms where the upstream Mercury build is not available, install a release build and either put `mercury` in `PATH` or set `MERCURY_MODEM_PATH`.

The GUI starts Mercury with the selected ARQ base port and broadcast port:

```sh
mercury -p 8300 -b 8100
```

Audio settings in the Modem tab map to Mercury options: sound system becomes `-x`, input device becomes `-i`, output device becomes `-o`, and RX channel becomes `-k`. Input/output devices are editable drop-downs populated from the OS audio device list; leave them on `Default` for Mercury defaults, or type a platform-specific device string such as `plughw:0,0`.

Extra modem arguments can be added in the Mercury Modem panel. For example, if you want Mercury itself to handle CAT/PTT:

```sh
./mercury -p 8300 -b 8100 -R <hamlib_model_id> -A <radio_device>
```

The default architecture is the opposite: Mercury runs without `-R/-A`, and Mercury Chat handles PTT through its hamlib CAT panel by following TNC `PTT ON` / `PTT OFF` events. Avoid enabling CAT/PTT in both Mercury and Mercury Chat at the same time.

The hamlib CAT panel lists radios by manufacturer/model name and stores the numeric hamlib model ID internally. RTS and DTR can be left unset or forced on/off before opening the CAT port. The PTT method can use normal CAT PTT, RTS, or DTR depending on the operator's interface.

Then launch:

```sh
./mercury-chat/build/mercury-chat
```

Set your callsign, connect to `127.0.0.1:8300`, initialize the station, and send or wait for a beacon.
