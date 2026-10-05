# Loader cleanup and recovery

[简体中文](../CLEANUP.md) | **English** · [English home](../../README.en.md)

Use **备份并清理 — Back up and clean up** in the current full package, or run its `Recover-Cleanup.cmd`. A separate [legacy cleanup tool v1.0.0](https://github.com/CreeperUX/AC8-Integrated-Mod/releases/tag/cleanup-v1.0.0) is also available for historical leftovers. Download the tool's ZIP, not its source archive.

## When cleanup is needed

Older packages relied on the original package's `active-session.json`. Moving or deleting that package, closing its console, or interrupting deployment/cleanup could leave `dwmapi.dll`, `ue4ss` or `steam_appid.txt` in the game's `Game\Binaries\Win64` directory.

Typical symptoms include `Existing loader conflict`, a remaining DLL after the UE4SS folder was removed, or an old cleaner reporting `No active candidate session` despite leftover files. Current cleanup supports retries after partial removal and reports unresolved leftovers. Optional save diagnostics or analysis are not allowed to masquerade as a successful game launch.

## Recovery steps

1. Exit AC8 normally and wait for its launcher. Cleanup refuses to run while AC8 is active.
2. Keep the cleanup/full package outside the game directory. Do not run it inside the ZIP.
3. In the GUI, check the game path and click **备份并清理**. Alternatively run `Recover-Cleanup.cmd`.
4. Confirm that the files belong to AC8 Integrated. The console interface may require the exact text `RECOVER`; cancel if their origin is unclear or another UE4SS mod uses them.
5. Wait for `CLEANUP COMPLETE: all three deployed items are absent`. Verified backups and `cleanup-report.json` are stored in the package's `cleanup-backups` directory.
6. Retry launch using the intended package's Steam entry or `Start.cmd`.

The current full package accepts game-root, Win64 and EXE paths. The standalone v1.0.0 tool expects the **game root**, containing `Game\Binaries\Win64\AceCombat8.exe`.

Cleanup is limited to the three recognized loader items in the selected game directory. It does not remove Windows system DLLs, game executables, saves, Steam settings or the extracted package. To stop using the mod entirely, also remove its Steam launch option.

## Ownership and backups

All selected contents are backed up and SHA-256 checked before removal. Failed backup or verification stops removal. An interrupted removal retains the backup and report so remaining files can be reviewed or retried.

Historical recovery requires recognized loader content and ownership evidence; the filename alone is not proof. Unknown loaders, missing or mismatched ownership, other mods, changed contents or filesystem links can block cleanup. User confirmation is still necessary.

Keep backups until normal launch is confirmed. Do not publicly upload all of `cleanup-backups`: it may contain logs or personal test data. Restore only while the game is closed and the destination is free of newly deployed files; never overwrite a newer installation.

| Message | Meaning |
|---|---|
| `Ownership marker missing/mismatched` | Ownership cannot be established; review the original package and other mods. |
| `Unknown dwmapi.dll hash` | The file is not the pinned loader; it may be modified or belong to another mod. |
| `Other UE4SS mods found` | The directory cannot safely be treated as this package's leftovers. |
| `Filesystem link refused` | The target or an ancestor is a link/junction crossing the cleanup boundary. |
| `Contents changed` / access denied | A file changed, is in use, or is inaccessible. Preserve the report and inspect the cause. |

Do not work around these errors by deleting system DLLs, disabling security software or taking ownership of unrelated directories.

## Read-only inspection

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Recover-Cleanup.ps1 -GameRoot "D:\SteamLibrary\steamapps\common\ACE COMBAT 8" -RecoverHistorical -CheckOnly
```

`-CheckOnly` validates without creating backups or removing files. The script flag `-RecoverHistorical` represents an explicit ownership decision by the caller; the interactive entry is preferred for ordinary use.

Isolated tests cover ownership checks, missing/partial state, retries, modified files, backup failures and directory links. They do not establish the origin of files on another player's computer or replace review of that machine's error report.
