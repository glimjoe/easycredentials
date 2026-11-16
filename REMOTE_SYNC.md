# Remote database synchronization

EasyCredentials backports the command-based remote sync interface from KeePassXC 2.8. It works with existing KDBX databases and does not require a new database format.

1. Open and unlock your local database.
2. Open **Database → Remote Sync → Setup Remote Sync**, or **Database Settings → Remote Sync**.
3. Create a named configuration with a download command and, optionally, an upload command. Each transfer command or its standard input must contain `{TEMP_DATABASE}`. This placeholder is replaced with the path of a temporary encrypted KDBX file.
4. Set a timeout that allows the connection and transfer to finish, for example 60 seconds for an Internet connection. Save the configuration, accept the database settings, and save your local database.
5. Select the configuration from **Database → Remote Sync**.

The commands and input use the same `KPXC_REMOTE_SYNC_SETTINGS` metadata key and JSON fields as KeePassXC 2.8. Configurations stay inside the encrypted database. A shared database can contain separate Windows and macOS configurations with the appropriate local paths for each device.

## Transfer commands

Commands are launched directly as programs, without a command shell. Quote paths that contain spaces. To use a script, invoke its interpreter explicitly, such as `powershell.exe ... -File "C:/private/sync.ps1" -Action Download -Database "{TEMP_DATABASE}"` on Windows, or `/bin/sh "/Users/me/.config/vault/sync.sh" Download "{TEMP_DATABASE}"` on macOS. Use the equivalent `Upload` command to transfer the merged file back.

SFTP is a suitable transport. Use a dedicated account that only has access to the vault directory, a separate SSH key for each device, and a pinned SSH host key. Keep transport credentials in private local files or an SSH agent; a database must not require its own unlock password to retrieve its transport key. An upload helper should transfer to a separate remote filename and rename it only after a successful upload, so an interrupted upload cannot truncate the current remote database.

## Merge and failure behavior

Synchronization downloads the remote file into a private temporary directory, opens it with the current database key, verifies the root-group identity, and merges both directions using the existing KeePassXC merger. The merged local database is saved before the merged transfer file is uploaded. Local changes are retained if uploading fails. A failed download, incompatible key, or unrelated database stops synchronization before either database is written. Temporary transfer files are removed when the operation finishes.

A configuration with an empty upload command downloads and merges into the local database only. Upload-only configurations are rejected because they cannot incorporate remote changes safely. Initial provisioning of an empty server should be done with your SFTP client or upload helper once, before using remote sync.

Both databases must use the same master password, key file, and hardware-key configuration. If their credentials differ, unlock the other copy separately and use the existing manual merge workflow to reconcile it.

## Security and concurrent edits

The server stores encrypted KDBX files. It does not need the database master password or the key file used to unlock the database. Use a strong, unique master password and retain a separate offline backup. Store any database key file outside the synchronized server directory.

Run only configurations you created or reviewed: transfer commands execute with your operating-system account's privileges. Synchronization is manual and does not automatically execute commands when a database is opened.

The complete download/merge/upload operation is not an atomic transaction. If two devices synchronize simultaneously, one upload can overwrite changes uploaded by the other after its download. Synchronize before editing and again after saving, and wait for one device's operation to finish before starting on the other. Keep server-side version history outside the transfer account's permissions so accidental overwrites and deletions can be recovered. Remote replacement by rename protects against partial uploads, but does not eliminate this concurrency limitation.
