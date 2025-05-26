### TODO for RISC-V AIA Selftest Support

This patch introduces initial support for testing RISC-V AIA interrupt virtualization. To align with the completeness of GICv3 selftests (`vgic_irq.c`, `vgic_init.c`, `vgic_lpi_stress.c`), the following work remains:

#### ✅ Basic Device and Address Configuration 
- [ ] Implement creation and validation of `KVM_DEV_TYPE_RISCV_AIA`.
- [ ] Test `KVM_DEV_RISCV_AIA_GRP_ADDR` attributes:
  - Alignment checks
  - Out-of-range IPA address rejection
- [ ] Validate hart-to-interrupt file address mappings.
- [ ] Ensure duplicate or overlapping interrupt file regions are rejected.

#### ✅ IRQ Injection and State Handling 
- [ ] Support interrupt injection paths:
  - [`KVM_SIGNAL_MSI`](https://docs.kernel.org/virt/kvm/api.html#kvm-signal-msi)
  - `KVM_IRQ_LINE`, `KVM_IRQFD` if supported by IMSIC
- [ ] Add support for level-triggered vs edge-triggered interrupts (if defined).
- [ ] Verify priority, preemption, and active state tracking:
  - Simulate nested interrupt preemption
  - Manipulate and verify active/pending states (`ISACTIVER`/`ISPENDR` equivalents)

#### ✅ LPI/MSI Stress Testing 
- [ ] Implement multi-threaded MSI injection using `KVM_SIGNAL_MSI`.
- [ ] Simulate ITS-like device-to-interrupt routing using AIA data structures.
- [ ] Test delivery/ack/eoi logic across multiple vCPUs.
- [ ] Add performance metrics (e.g., LPIs per second).

#### ✅ Common Infrastructure and Code Cleanup
- [ ] Replace `gic_init()` / `its_init()` with `aia_init()` or equivalent.
- [ ] Remove GIC-specific constants (e.g., `GICR_TYPER`, `IAR_SPURIOUS`, `DIR`).
- [ ] Avoid including ARM-specific headers (`gic.h`, `gic_v3.h`).
- [ ] Ensure all test logic is RISC-V AIA-compliant and architecture-neutral.

---

This checklist ensures feature and test coverage parity between ARM's GICv3 selftests and RISC-V AIA, and guides future contributions for robust AIA virtualization validation.
