# BLE connection

## Scope

The connection policy remains independent of a particular BLE stack. A concrete
ESP-IDF/NimBLE adapter now supplies the microcontroller integration. CAN packet
parsing is still outside this component.

## Components

`DashKitConnection` owns the connection policy:

- scan for the exact advertised name `DashKit`;
- connect to the discovered peripheral;
- establish an encrypted, bonded connection;
- discover the DashKit CAN service;
- read and validate the DashKit firmware version;
- subscribe to CAN notifications;
- enable the DashKit application keepalive only on compatible firmware;
- remove one stale peer bond and retry pairing once;
- enforce a timeout for every asynchronous stage;
- retry failed setup a bounded number of times;
- automatically reconnect after a previously healthy link is lost;
- forward only CAN-characteristic notifications after subscription succeeds.

`BleCentral` is the driven platform port. `EspNimbleCentral` implements it with
NimBLE. Host tests implement it with a deterministic fake.

`DashKitConnectionObserver` is the output boundary. It reports connection state
and passes received CAN packet bytes to the next anti-corruption-layer stage.
The payload span is valid only for the duration of the callback; a consumer
that queues it must copy the bytes into bounded owned storage.

## State machine

```text
Idle
  | start
  v
Scanning -> Connecting -> Securing -> Discovering -> ReadingVersion
   |             |            |             |                |
   |             |            |             v                v
   |             |            |          Subscribing ----> Ready
   |             |            |                              |
   +-------------+------------+------------------------------+
                                failure / timeout / link loss
                                              |
                                              v
                                      WaitingToRetry
                                         |       |
                                      timeout  budget spent
                                         |       |
                                         v       v
                                      Scanning  Failed
```

Calling `stop()` from any active state returns to `Idle`, cancels the relevant
platform operation, and disables automatic retry. Calling `start()` after a
terminal failure creates a fresh retry budget.

All port calls and callbacks must be serialized on one event-loop context. The
class intentionally has no internal locking, task, or allocator policy.

## Default policy

| Setting | Default |
| --- | ---: |
| Setup attempts | 3 |
| Scan timeout | 15 seconds |
| Connect timeout | 10 seconds |
| Security timeout | 30 seconds |
| Service-discovery timeout | 10 seconds |
| Firmware-version read timeout | 5 seconds |
| Subscription timeout | 10 seconds |
| Keepalive interval | 15 seconds |
| Keepalive write timeout | 5 seconds |
| Delay between attempts | 2 seconds |

These values are based on upstream DashPilot behavior and conservative pairing
allowances. Basic connection, security, service discovery, subscription, and
notification delivery have been exercised against DashKit hardware. The values
remain configurable; long-running reconnect, pairing-recovery, MTU, and
notification-rate measurements are still required.

## NimBLE adapter responsibilities

The ESP-IDF adapter:

1. initializes NimBLE central, observer, and peripheral roles plus NVS-backed
   bond storage; the peripheral role supplies the ATT server needed to answer
   DashKit's peer-initiated MTU exchange;
2. scans by complete/shortened local name because DashKit does not advertise its
   service UUID;
3. stops scanning before initiating a connection;
4. prefers the 1M PHY, accepts DashKit's ATT MTU negotiation, and logs the
   negotiated value;
5. initiates “Just Works” LE Secure Connections pairing or reuses a stored bond;
6. discovers the CAN service, firmware-version, control, and notification
   characteristics as the state machine requests them;
7. reads `CADA0005`, writes the notification CCCD, and writes keepalive
   commands to `CADA0004` without allocating;
8. handles NimBLE repeat-pairing events by replacing only the stale peer bond;
9. copies NimBLE notification data into a bounded event queue;
10. translates stack-specific status codes into `BleFailure` values;
11. drains callbacks on the same task that owns `DashKitConnection`.

The firmware entry point initializes NVS and NimBLE, starts the connection
immediately, and services adapter events and connection deadlines every 10 ms.
The adapter queue holds 16 fixed-size events, and each notification event can
hold at most the requested 512-byte ATT payload. Queue overflow is logged and
the new event is dropped; the callback never allocates or blocks waiting for
space.

## Firmware-aware keepalive

DashKit firmware `0.1.2` introduced opcode `0x45`. Tesla Head-Up Display reads the plain-text
semantic version from `CADA0005` and enables keepalive only for version `0.1.2`
or newer. It writes `[0x45, 0x00]` immediately after subscribing and every 15
seconds after a successful write. DashKit arms its 60-second watchdog only
after the first ping, so an absent, failed, or malformed version read leaves
keepalive disabled and preserves compatibility with older firmware.

A failed keepalive write or a write that remains incomplete for five seconds
reconnects the whole link. This avoids leaving the connection apparently ready
while DashKit's already-armed watchdog is approaching expiry.

## Pairing recovery

Both sides persist BLE keys. On the first security failure in a connection
cycle, Tesla Head-Up Display removes only the active DashKit peer bond (when one exists),
disconnects, and retries with fresh keys. NimBLE repeat-pairing events receive
the equivalent peer-scoped delete-and-retry response. Recovery is bounded; a
second failure is reported as `PairingRequired` rather than repeatedly erasing
keys.

This cannot bypass DashKit's security policy. If DashKit still has another
bond, an already-paired client must send `VC_CMD_ENTER_PAIRING` to open its
two-minute window, or the DashKit must be factory-reset. Tesla Head-Up Display intentionally
does not expose that vehicle-control command automatically.

CAN packet parsing remains a separate decoder component.

## Hardware status

The H619 firmware has connected to a physical DashKit, completed the encrypted
GATT setup, and received CAN notifications. This confirms the central role,
peer-initiated MTU response path, service discovery, and subscription flow.
Per-signal vehicle validation, repeated bond recovery, reconnect after radio
loss, the negotiated MTU under every supported firmware version, and the
60-second keepalive behavior still require recorded endurance runs.

## HUD connection and freshness feedback

The DashKit adapter maps connection snapshots into application-level status
without exposing BLE types to the dashboard. Initial setup is shown as
`CONNECTING`; retries as `RECONNECTING`; exhausted pairing recovery as
`PAIRING REQUIRED`; and other terminal failures as `CONNECTION FAILED`.

After reaching `Ready`, the HUD shows `WAITING FOR DATA` until a valid DashKit
packet is accepted. The message disappears while valid telemetry is fresh and
becomes `NO DATA` after 500 ms without another valid packet. Disconnect and
failure states take precedence over telemetry freshness. The policy uses the
same monotonic clock as telemetry ingestion and requests a render only when the
visible state changes.
