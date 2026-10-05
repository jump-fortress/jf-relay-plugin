# Jump Fortress Spectator Commands Plugin

A 64-bit TF2 client plugin plus a Node.js companion bridge app.

## Setup

1. Download `jf_spec.dll` (Windows) or `jf_spec.so` (Linux) from the GitHub Actions
   build artifacts and place it in your TF2 client's `tf/addons/` directory.
2. Launch **64-bit TF2 with `-insecure`**. In the game console *before* joining an STV server:

   ```cfg
   plugin_load addons/jf_spec
   ```

3. Configure `companion/.env` (copy `.env.example` if missing):

4. Start the companion **on the same OS as TF2**. For Windows TF2, use PowerShell,
   not WSL. Use Node 24+:

   ```powershell
   cd C:\jf-relay-plugin\companion
   npm start
   ```

5. Join the playback relay's SourceTV. With an active progress feed, add binds. E.g:

   ```cfg
   bind F1 "jf_spec_rank 1";
   bind F2 "jf_spec_rank 2";
   bind F3 "jf_spec_rank 3";
   // jf_spec_rank <N>
   ```

   ```cfg
   // Last place:
   bind F4 "jf_spec_last";
   ```

   ```cfg
   bind F5 "jf_spec_prev_rank";
   bind F6 "jf_spec_next_rank";
   ```

Switching is manual: each key picks from the latest progress ranking. An empty,
stale or wrong-map feed leaves the camera unchanged. `jf_spec_status` helps diagnose
the plugin. The companion console should report both progress and IPC connections.

