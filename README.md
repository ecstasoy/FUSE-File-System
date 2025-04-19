# FUSE Filesystem Implementation

This is a FUSE implementation project that allows users to create and manage custom filesystems in user space.

## Project Structure

```
.
├── src/                # Source code directory
│   ├── filesystem.c    # Core filesystem implementation
│   ├── misc.c         # Utility functions
│   └── fuse.c         # FUSE interface implementation
├── include/           # Header files directory
├── test/             # Unit tests directory
├── diskfmt.py        # Disk formatting tool
├── gen-disk.py       # Disk image generation tool
├── read-img.py       # Disk image reading tool
└── Makefile          # Project build file
```

## System Requirements

- macOS operating system
- Python
- FUSE for macOS (installable via Homebrew)
- GCC compiler
- Check unit testing framework (installable via Homebrew)

## Installing Dependencies

On macOS, you can install the necessary dependencies using Homebrew:

```bash
brew install osxfuse
brew install check
```

## Building the Project

Build the project using the following command:

```bash
make all
```

This will generate the following files:
- `fuse`: FUSE filesystem executable
- `unittest-1`: Unit test suite 1
- `unittest-2`: Unit test suite 2
- `test.img`: Test disk image 1
- `test2.img`: Test disk image 2

## Running Tests

Run the unit tests:
```bash
./unittest-1
./unittest-2
```

## Usage

1. Generate a disk image:
```bash
python gen-disk.py -q disk1.in test.img
```

2. Read disk image contents:
```bash
python read-img.py test.img
```

3. Mount the filesystem:
```bash
./fuse [mount_point] [disk_image_file]
```

## Cleaning Up

Clean build files:
```bash
make clean
```

## Notes

- Ensure you have appropriate permissions to mount the filesystem
- This filesystem is intended for educational and testing purposes only 