<div align="center">
  <h1>Filesystem & Storage Architecture</h1>
  <p><em>Virtual File System layer, partition schemes, and FAT32 management in BoredOS.</em></p>
</div>

---

BoredOS implements a Virtual Filesystem (VFS) layer to support system binaries, initial RAM disks, user scripts, and physical drive volumes.

## 1. Virtual File System (VFS)

The Virtual File System abstractly routes standard POSIX path targets to distinct filesystems mounted across the directory tree.

Key VFS functionalities include:
-   **File Descriptors**: Mapping process-local integers to open file structures.
-   **Standard Operations**: Routing `open()`, `read()`, `write()`, `close()`, `seek()`, `poll()`, and directory listings (`vfs_list_directory`) to backing mount drivers.
-   **Path Resolution**: Normalizing relative paths and resolving symbolic linkages.
-   **SMP Safety**: Locking modifications and I/O tasks via spinlocks to avoid context corruption across multiple cores.

---

## 2. Mounting and Partition Layouts

BoredOS automatically scans and registers disks and partitions during device initialization:

### Partitions
The `Disk Manager` supports both legacy **MBR (Master Boot Record)** partition schemes and modern **GPT (GUID Partition Table)** schemes.
-   Partition metadata and block boundaries are scanned automatically.
-   Partitions are mounted within `/dev` as block nodes (e.g. `/dev/sda1`, `/dev/sda2`).
-   The default filesystem format is **FAT32**.

### Backing Controllers
-   **AHCI SATA DMA**: Probes and reads/writes to SATA drives with high-throughput DMA transfers.
-   **Legacy IDE PIO**: Fallback controller driver for older emulator setups.

---
