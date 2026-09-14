# nRF54LM20A bring-up plan (evidence-gated)

Status: **not started and not startable from the current evidence.** This plan
lists the gates that must open first, in order, and what each gate needs. No
step here flashes, signs or issues a device command in this task.

## Gate 0 — evidence (open now: NO)

| Need | Blocks | Source that would open it |
|---|---|---|
| Board pin table: every GPIO/ADC/SPI/TWI/net mapping | every peripheral | vendor schematic, netlist, board file or devicetree; or X-ray/CT + continuity |
| Blind/buried via span | inner-layer connectivity | stack-up/via drawing, or vendor board file |
| nPM1300 charge current, termination voltage, rail voltages, NTC | **safety** | cell datasheet + vendor charger config, or an authorized read-only register dump |
| Sensor variant per image and its register config | sensor bring-up | vendor release note / variant-tagged image |
| Switch H=0.6 mm drawing and IR drive | button bring-up | vendor mechanical/electrical drawing |

The supplied PCB production packages contain **Gerber + drill + BOM + placement
+ silkscreen/drill PDF only** (see `hardware_evidence/gerber_layer_map.md`,
`HW-SRC-001`). They cannot open Gate 0.

## Gate 1 — project-local SDK

Follow `docs/toolchain_inventory.md`. Deliverable: a project-local NCS/Zephyr
tree with `west` and the `arm-zephyr-eabi` toolchain, no global change. Exit
criterion: `west build -b native_sim` of `zephyr/` succeeds with
`-DG6HE_ENABLE_HARDWARE=OFF`.

## Gate 2 — host/native_sim bring-up

With Gate 1 open, build and run the target entry with the mock backends. This
validates the module graph, the event/HID plumbing and the mocks. It touches no
hardware and needs no pin evidence.

## Gate 3 — board definition

Only after Gate 0 supplies pins:

1. Create a project-local board under `zephyr/boards/<vendor>/<board>/` with a
   `*_defconfig`, `*_cpuapp.dts` and `board.yml`, modelled on Nordic's nRF54L
   DK files and the nRF54LM20A SoC devicetree.
2. Fill the pin table in `include/g6he_board.h`, each line annotated with the
   evidence id, and set `G6HE_BOARD_PIN_TABLE_COMPLETE 1`.
3. Provide the power values and set `G6HE_BOARD_ELECTRICAL_COMPLETE 1` **only
   with the cell datasheet and vendor charger configuration in hand**. Charge
   current and termination voltage are safety parameters.

Example overlay shape (values intentionally omitted — they must come from
evidence, not this document):

```dts
/ {
    chosen { zephyr,console = &uart30; };

    buttons {
        compatible = "gpio-keys";
        /* left_ir: gpios = <&gpioX N GPIO_ACTIVE_...>;  HW-U-013 */
    };

    vbatt {
        compatible = "voltage-divider";
        io-channels = <&adc 0>;   /* NTC / VBAT channel: HW-U-020 */
    };
};

&spi00 {
    status = "okay";
    cs-gpios = <&gpioX N GPIO_ACTIVE_LOW>;   /* HW-U-011 */
    paw3955: paw3955@0 {
        compatible = "pixart,paw3955";        /* binding to be added */
        reg = <0>;
        /* MOTION/NRESET GPIOs: HW-U-011 */
    };
};

&i2c30 {
    status = "okay";
    /* nPM1300 is bound through the official npm1300 driver; address and
       interrupt are HW-U-012. Do NOT add charge/termination values until
       HW-U-030..HW-U-034 are closed. */
};
```

## Gate 4 — safe power sub-system

Implement `g6he_power` against the official NCS `npm1300_charger` /
`npm1300_regulator` / `npm1300_fuel_gauge` drivers. Until the battery datasheet
and charger configuration exist, keep the electrical constants at
`G6HE_ELECTRICAL_UNKNOWN`; the build will refuse to enable them.

## Gate 5 — input stack

Wire `g6he_optomagnetic` to the IR GPIO and the Hall ADC channel, and
`g6he_sensor` to the SPI peripheral. Calibrate the Hall curve only from
authorized measurement; the compiled 114/120-element tables are **not**
recovered with reproducible boundaries (see the final report) and must not be
transcribed.

## Gate 6 — USB/BLE/2.4G

Implement `g6he_hid` on the USB device stack and `g6he_wireless` on the
BLE/2.4G stacks. The vendor update protocol (`g6he_protocol`) is already
modelled and verified against the real `+84`/`+87` containers.

## Gate 7 — build, sign, flash (out of scope here)

Signing and flashing are separate, owner-authorized activities with an official
pipeline. This repository must never be the place where a `.signed.bin` is
produced or a flash command is defined. Any image built here remains
`ENGINEERING-UNSIGNED-NOT-FOR-FLASHING`.

## Ordering rule

Gates are **sequential**. In particular Gate 3 (pins) and Gate 4 (power) may
not be worked around by placeholder values: a placeholder that compiles is worse
than a compile error, because it can be flashed. The compile-time blockers in
`include/g6he_board.h` implement this rule.
