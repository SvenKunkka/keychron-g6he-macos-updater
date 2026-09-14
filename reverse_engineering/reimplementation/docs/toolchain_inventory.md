# Toolchain and SDK inventory (read-only)

Observed on the build host at the time of this run. No global or system change
was made; nothing was installed.

| Tool | Present | Version / path | Notes |
|---|---|---|---|
| `clang` | yes | Apple clang 17.0.0 (clang-1700.6.4.2), `/usr/bin/clang` | build + static analyzer |
| `cc` | yes | aliases clang | |
| `make` | yes | GNU Make 3.81 | host build |
| `cmake` | yes | 4.4.3 (Homebrew) | **newer than most NCS releases expect** |
| `ninja` | **no** | — | Zephyr's default generator |
| `dtc` | yes | DTC 1.8.1 | devicetree compiler |
| `arm-none-eabi-gcc` | yes | Arm GNU Toolchain 15.2.Rel1 | bare-metal, **not** the Zephyr SDK |
| `arm-zephyr-eabi-gcc` | **no** | — | Zephyr SDK cross-compiler |
| `openocd` | **no** | — | debug/flash (not needed; flashing is out of scope) |
| `nrfutil` | **no** | — | SDK/toolchain manager |
| `west` | **no** | — | Zephyr meta-tool |
| `python3` | yes | 3.14.7 (Homebrew) | **some Zephyr releases pin 3.10–3.12** |
| `bsdtar` | yes | `/usr/bin/bsdtar` | extracted the RAR5 packages |
| `7zz` | yes | 7-Zip 26.03 | archive listing |
| `pdftotext` | yes | Homebrew poppler | datasheet text extraction |
| NCS tree | **no** | no `~/ncs`, `~/zephyrproject`, `~/.west`, `/opt/nordic` | |
| Zephyr tree | **no** | — | |
| `ZEPHYR_BASE` | unset | — | |
| Xcode CLT | yes | `/Library/Developer/CommandLineTools` | |

## Conclusion

There is **no official local Zephyr or nRF Connect SDK**, so a `native_sim`
Zephyr build could **not** be produced, and no nRF54LM20A target build was
attempted (the board data is in any case insufficient — see
`docs/nrf54lm20a_bringup_plan.md`). The portable modules are instead built and
tested on the host with the `G6HE_ENABLE_HARDWARE=0` configuration, which is the
same translation-unit set the Zephyr project lists.

## Exact official project-local installation plan

This plan installs everything **inside** the project's private working
directory. It makes no global change and does not touch the device. It was
**not executed** in this run.

The current official mechanism is the nRF Connect SDK's *pre-packaged SDK and
toolchain* (NCS ≥ 3.3.0 supports the nRF54LM20A SoC). Two supported routes:

### Route A — `nrfutil` SDK manager (preferred; no global west bootstrap)

```bash
# 1. Private root (choose a directory outside any git repository)
export G6HE_PRIVATE_ROOT="/absolute/path/to/private-g6he-work"
mkdir -p "$G6HE_PRIVATE_ROOT/ncs"

# 2. Install nrfutil project-locally (single binary; no system package)
#    https://www.nordicsemi.com/Products/Development-tools/nRF-Util
#    Verify the downloader signature/hash from Nordic before use.

# 3. Install NCS v3.3.0 (SDK + matching toolchain) into the private root
nrfutil sdk-manager install v3.3.0 \
    --sdk-path "$G6HE_PRIVATE_ROOT/ncs/v3.3.0" \
    --type nrf
```

The pre-packaged install includes `west`, `ninja`, the Python runtime and the
`arm-zephyr-eabi` toolchain, so no separate toolchain step is needed. Note this
download is several GB.

### Route B — `west` in a private venv

```bash
python3 -m venv "$G6HE_PRIVATE_ROOT/ncs-venv"
. .../ncs-venv/bin/activate
pip install west
cd "$G6HE_PRIVATE_ROOT/ncs"
west init -m https://github.com/nrfconnect/sdk-nrf --mr v3.3.0
west update
west zephyr-export
pip install -r zephyr/scripts/requirements.txt
```

Route B needs an explicit Zephyr SDK install and `ZEPHYR_SDK_INSTALL_DIR`
set to a project-local path.

### Then, and only then

```bash
cd ".../reimplementation/zephyr"
west build -b <official-or-project-local nrf54lm20a board target> -- \
    -DG6HE_ENABLE_HARDWARE=OFF
```

`-DG6HE_ENABLE_HARDWARE=ON` is expected to **fail** until the pin table and
electrical constants are evidenced. A `native_sim` build uses
`-b native_sim` with the same `G6HE_ENABLE_HARDWARE=OFF`.

No build step in this project signs an image or exposes a flash command.

## Environment risks recorded

* Python 3.14 is newer than several Zephyr LTS requirement pins; the
  pre-packaged NCS toolchain ships its own Python, which avoids this.
* CMake 4.4.3 is newer than the minimum some NCS releases accept; the
  pre-packaged toolchain also pins CMake.
* No `openocd` is present, which is consistent with this task's no-flash
  boundary and is not needed for compilation or testing.
