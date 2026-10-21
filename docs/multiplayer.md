# Two-player LAN campaigns

The native game supports two players on the same local network. The host runs the strategic simulation and resolves
battles; the guest sends orders and receives the host's game state. Both players use the ordinary strategic interface.

## Start a campaign

1. Use the same native build and matching game data on both computers.
2. Open **Play Head to Head** from the main menu.
3. On one computer, choose **Host a game**, enter a campaign name, and host the session. Choose the host's faction,
   galaxy size, and victory conditions.
4. On the other computer, choose **Join a game**, select the nearby session, and connect. If discovery does not find
   it, enter the host's displayed IPv4 address. A custom port can be entered as `address:port`.
5. Both players choose **Ready**. The host selects **Start campaign**.

Allow the game to access the local network when the operating system asks. Some guest Wi-Fi networks isolate devices;
both computers need a network that permits them to communicate. The default port is **47624**, using UDP for discovery
and TCP for the game. No account, external server, or router port forwarding is needed for LAN play.

The guest has the other faction. Settings changes clear readiness so both players can review them before starting.
The connection check reports mismatched builds or game data before a campaign starts.

## Save and resume

The host creates an autosave every **five minutes of connected, unpaused play**, retaining the latest **ten autosaves
per campaign**. Manual checkpoints are retained separately. Checkpoints wait for a complete strategic step; during a
battle they wait until strategic play resumes. Leaving during a battle retains the last completed checkpoint.
The window title shows connection and save status.

Press **F8** to create a manual checkpoint. Either player can request one; the host writes it and transfers a backup
to the guest. The in-game Save controls also create campaign checkpoints. Existing checkpoints appear in the in-game
options screen; loading a LAN checkpoint is done from the multiplayer lobby.

To resume later, host a session, connect the other player, select a checkpoint in the lobby, and both choose Ready.
The host selects **Resume selected**. The guest receives the saved campaign automatically; files and local slot
numbers do not need to match. Use the checkpoint pager to find older saves, or **New** to deselect a checkpoint and
start a new campaign. A guest's backup can also be used to host a later session, retaining the saved faction assignment.

Checkpoints are stored under the platform's Rebellion user configuration directory, in `Multiplayer/`:

| Platform | Default directory |
|---|---|
| macOS | `~/Library/Application Support/Rebellion/Multiplayer/` |
| Linux | `$XDG_CONFIG_HOME/rebellion/Multiplayer/`, or `~/.config/rebellion/Multiplayer/` |
| Windows | `%APPDATA%\Rebellion\Multiplayer\` |

Optional settings before the `[registry]` section in `config.ini`:

```ini
multiplayer_save_dir=/path/to/campaign-checkpoints
multiplayer_autosave_seconds=300
```

The autosave interval accepts 1–3600 seconds. Invalid values use five minutes. Checkpoints include a checksum and the
build/data identity; resume requires matching game data and a supported save format. Both connected computers need
the same current build. Original single-player and historical DirectPlay save slots are separate from these campaign checkpoints.

## Interrupted connections

The campaign stops advancing when the other computer stops responding. A guest whose game remains open reconnects
automatically and receives a fresh host snapshot before sending more orders. **F9** retries the connection manually.
If either game has closed, resume a saved checkpoint through a new lobby session. If the guest restarts during strategic
play, the host can press F8 to capture the current position before returning to the lobby. There is no automatic host migration.

Orders interrupted by a disconnect are not replayed automatically. Check the current state after reconnecting and
repeat an order if needed. State updates preserve surviving game-object addresses so open strategic panels can remain
open during play. Reconnection replaces the guest's world and reopens its strategic screen.

## Development and validation

Existing single-player save files retain their format and remain loadable. Memory serialization uses a vector-backed
writer and a span reader behind the game's existing typed Read/Write interface. Compatibility tests compare complete
game-state output byte for byte with the existing file writer.

The implementation uses native sockets in `src/port/native/lan_transport.*`; framing and bounds are independent of
the game serializer. `LanGame` owns the session and checkpoint policy. `LanPlayer` represents a human faction whose
interface is on the guest. `WorldReplicaState` reconciles all three game-object views by key, preserving surviving
addresses and retaining deleted objects while UI references can still exist. The guest never runs the strategic step
driver or battle simulator.

Build with the top-level Clang/C++23 CMake project. Run the transport, wire, and checkpoint unit tests with:

```sh
ctest --test-dir build/native -R 'native_lan|native_campaign' --output-on-failure
```

Run two real game processes on loopback with isolated checkpoint directories:

```sh
REBELLION_CONFIG="$PWD/work/macos/config.ini" uv run tools/lan_smoke.py \
  build/native/Rebellion.app/Contents/MacOS/Rebellion --output work/lan-smoke
```

Pass `--resume /absolute/path/to/checkpoint.rcp` to check a later session. The smoke exercises a guest rename order,
host simulation, replicated ticks, connection retry, automatic and manual saves, and checkpoint transfer. Add
`--battle --host-side 2` to exercise guest battle decisions with the factions reversed, and `--endgame` to check
the paired victory/defeat transition. It writes
process logs and JSON reports under its output directory. It does not establish physical Wi-Fi reachability or validate
another operating system.

Set `REB_LAN_REPLICA_SMOKE=1` when running `tools/modern_smoke.py` to exercise real object updates, stable addresses,
creation, deletion, reparenting, malformed packets, and serialization roundtrips. `REB_LAN_LOBBY_SMOKE=/absolute/output`
runs the native lobby rendering smoke and captures Host/Join BMPs.
