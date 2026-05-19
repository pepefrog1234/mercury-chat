# Mercury Chat SQLite schema

Mercury Chat stores received beacon events and completed chat text messages in SQLite.
When an ARQ link is established, the chat window loads the latest 500 stored
messages for that peer callsign from this database before showing the new
connection status line.

## Database path

When the GUI is launched with `--settings-file` or a named profile, the database is stored beside that INI file using the same basename:

```text
profiles/a.ini      -> profiles/a.sqlite3
profiles/b.ini      -> profiles/b.sqlite3
```

Without a settings file, the database is stored in Qt's `AppDataLocation` as `mercury-chat.sqlite3`.

All timestamps are UTC ISO-8601 strings with milliseconds, for example `2026-05-19T01:23:45.678Z`.

## PRAGMA

```sql
PRAGMA user_version = 1;
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
```

## beacon_events

One row is written for every received `CQFRAME` beacon that is not from the local callsign.

```sql
CREATE TABLE beacon_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    heard_at TEXT NOT NULL,
    profile TEXT,
    local_call TEXT,
    remote_call TEXT NOT NULL,
    bandwidth_hz INTEGER NOT NULL,
    snr_db REAL,
    has_snr INTEGER NOT NULL DEFAULT 0 CHECK(has_snr IN (0, 1)),
    source TEXT NOT NULL DEFAULT 'tnc_cqframe',
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);

CREATE INDEX idx_beacon_events_remote_time
    ON beacon_events(remote_call, heard_at);
```

Fields:

- `heard_at`: when the GUI handled the beacon.
- `profile`: UI profile name, such as `A` or `B`.
- `local_call`: local callsign at the time of reception.
- `remote_call`: beacon callsign.
- `bandwidth_hz`: beacon bandwidth advertised by Mercury.
- `snr_db`: most recent SNR telemetry if it was fresh.
- `has_snr`: `1` when `snr_db` came from recent telemetry, otherwise `0`.
- `source`: currently always `tnc_cqframe`.

## chat_messages

One row is written for each complete incoming or outgoing text message. Partial receive previews and typing indicators are not written.

```sql
CREATE TABLE chat_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    message_at TEXT NOT NULL,
    profile TEXT,
    direction TEXT NOT NULL CHECK(direction IN ('in', 'out')),
    local_call TEXT,
    remote_call TEXT,
    link_source TEXT,
    link_destination TEXT,
    bandwidth_hz INTEGER,
    body TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);

CREATE INDEX idx_chat_messages_remote_time
    ON chat_messages(remote_call, message_at);

CREATE INDEX idx_chat_messages_time
    ON chat_messages(message_at);
```

Fields:

- `message_at`: when the GUI logged the completed message.
- `profile`: UI profile name, such as `A` or `B`.
- `direction`: `out` for local sent text, `in` for received text.
- `local_call`: local callsign at the time of logging.
- `remote_call`: peer/operator callsign when known.
- `link_source`: source callsign reported by the current ARQ link.
- `link_destination`: destination callsign reported by the current ARQ link.
- `bandwidth_hz`: bandwidth reported for the current ARQ link.
- `body`: UTF-8 chat text.
