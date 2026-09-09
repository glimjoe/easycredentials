# EasyCredentials

EasyCredentials is a Windows-focused developer credential manager based on
[KeePassXC 2.7.12](https://github.com/keepassxreboot/keepassxc). It keeps KeePassXC's encrypted KDBX storage and
adds targeted entry templates for website accounts, databases, SSH, APIs, HTTPS certificates, and custom entries.

> EasyCredentials is an independent fork and is not an official KeePassXC release.

## Download

Unsigned Windows preview builds and SHA-256 checksum files are published on the
[Releases](https://github.com/glimjoe/easycredentials/releases) page.

See [EASYCREDENTIALS.md](EASYCREDENTIALS.md) for the current development and compatibility boundary.

## Features inherited from KeePassXC

EasyCredentials retains KeePassXC's core password-management capabilities and adds developer-focused entry templates.

### Basic
* Create, open, and save databases in the KDBX format (KeePass-compatible with KDBX4 and KDBX3)
* Store sensitive information in entries that are organized by groups
* Search for entries
* Password generator
* Auto-Type passwords into applications
* Browser integration with Google Chrome, Mozilla Firefox, Microsoft Edge, Chromium, Vivaldi, Brave, and Tor-Browser
* Support for passkeys using the browser integration
* Entry icon download
* Import databases from CSV, 1Password, Bitwarden, Proton Pass, and KeePass1 formats

### Advanced
* Database reports (password health, HIBP, and statistics)
* Database export to CSV, XML, and HTML formats
* TOTP storage and generation
* Field references between entries
* File attachments and custom attributes
* Entry history and data restoration
* YubiKey/OnlyKey challenge-response support
* Command line interface (keepassxc-cli)
* Auto-Open databases
* KeeShare shared databases (import, export, and synchronize)
* SSH Agent integration
* FreeDesktop.org Secret Service (replace Gnome keyring, etc.)
* Additional encryption choices: Twofish and ChaCha20

For a full list of changes, read the [CHANGELOG](CHANGELOG.md) document. \
For a full list of keyboard shortcuts, see [KeyboardShortcuts.adoc](./docs/topics/KeyboardShortcuts.adoc)

## Building EasyCredentials

The current supported target is Windows x64. Detailed upstream build instructions are available in
[INSTALL.md](./INSTALL.md), and the repository's `Windows build` workflow provides the reference MSVC/vcpkg build.

## Contributing

EasyCredentials issues and feature requests belong in this repository's
[issue tracker](https://github.com/glimjoe/easycredentials/issues). Upstream KeePassXC issues should still be reported
to the [KeePassXC project](https://github.com/keepassxreboot/keepassxc/issues).

## License

EasyCredentials modifications are licensed under GPLv3. The project is based on KeePassXC, whose code is licensed
under GPL-2 or GPL-3. Additional licensing for third-party files is detailed in [COPYING](./COPYING).
