# EasyCredentials development baseline

EasyCredentials is a Windows-focused developer credential manager based on KeePassXC 2.7.12.

## First implementation slice

- Keep the KDBX 3/4 storage format and KeePassXC encryption, locking, history, password generator, attachments, recycle bin, import/export, and KeeShare behavior.
- Add targeted entry templates for website accounts, database credentials, SSH credentials, API credentials, HTTPS certificates, and generic custom entries.
- Store the template identifier and template-specific values as ordinary KDBX custom attributes.
- Store SSH keys and certificate files as ordinary encrypted KDBX attachments, using the existing attachment import, preview, and export workflow.
- Keep multiple databases independently unlockable; EasyCredentials does not force other databases to lock when one is opened.

## Compatibility boundary

The first slice changes the visible application name and Windows product metadata, but deliberately keeps the existing executable name, configuration paths, IPC identifiers, browser integration identifiers, SSH Agent conventions, KeeShare format, and KDBX schema. This allows existing KeePassXC databases and settings to continue to work while the fork is still being established.

A later packaging milestone must give EasyCredentials its own executable, installer upgrade identity, configuration migration, native-messaging registration, and file-association policy before it is distributed alongside KeePassXC.

## Windows cloud build

The `Windows build` GitHub Actions workflow builds the project with MSVC and vcpkg, runs the unit and GUI tests, and uploads an unsigned portable ZIP plus its SHA-256 file. It runs for pushes to `main` and `feature/**`, for pull requests, or manually from the repository's Actions page.

The initial workflow intentionally does not publish an MSI. Until EasyCredentials has its own executable and installer upgrade identity, an MSI could conflict with an installed KeePassXC copy.

## License

EasyCredentials modifications are licensed under GPLv3. The project is based on KeePassXC, whose code is licensed under GPL-2 or GPL-3, with additional third-party licenses documented in [COPYING](COPYING).
