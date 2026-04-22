# zhamel-zfskey-kmod

`zhamel-zfskey-kmod` is the FreeBSD kernel module that consumes the
`zhamel_zfs_key` preload payload prepared by `zhamel`, imports the root pool if
needed, and loads the native OpenZFS wrapping key into the target encryption
root during early boot.

The module installs as:

```text
/boot/modules/zhamel_zfskey.ko
```

## Scope

This repository only contains the kernel module source and its release
packaging scaffold. It is intentionally separate from:

- `zhamel`, the EFI loader
- `foji-bsd`, the ports tree

## Build

Build against a matching FreeBSD source tree:

```sh
make SYSDIR=/path/to/freebsd-src/sys
```

Typical local example:

```sh
make SYSDIR=$HOME/devel/system/freebsd-src/sys
```

Install on a test machine:

```sh
doas make SYSDIR=/path/to/freebsd-src/sys install
```

## Notes

- The module depends on the in-tree OpenZFS headers from the target FreeBSD
  source tree.
- The source tree used at build time should match the kernel/OpenZFS version
  you plan to run.
- The loader is expected to preload the binary blob under the
  `zhamel_zfs_key` module info type before kernel handoff.

## Release Artifacts

Tagged releases on `main` produce source tarballs through GitHub Actions.
