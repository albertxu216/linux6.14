### TODO：RISC-V AIA 自测支持（Selftest Support）

本测试框架初步参考了 ARM GICv3 的自测实现，后续需补全以下部分以实现对 RISC-V AIA（IMSIC/MSI）完整功能的验证：

#### ✅ 基础功能验证
- [ ] 实现 `KVM_DEV_TYPE_RISCV_AIA` 设备的创建与有效性检测；
- [ ] 支持并测试 `KVM_DEV_RISCV_AIA_GRP_ADDR` 属性设置：
  - 对齐检查；
  - 地址越界（IPA限制）检查；
  - 重复 / 重叠配置的冲突检测；
- [ ] 多 Hart 场景下，验证中断文件地址映射是否正确生效；

#### ✅ 中断注入与状态处理
- [ ] 支持 AIA 中断注入路径：
  - `KVM_SIGNAL_MSI`（强制支持）；
  - 如果 IMSIC 支持，则加入 `KVM_IRQ_LINE`、`KVM_IRQFD` 的路径；
- [ ] 添加优先级控制与中断抢占测试（如模拟嵌套中断）；
- [ ] 测试 active/pending 状态恢复功能（如模拟迁移）；
- [ ] 支持电平敏感 / 边沿触发中断的注入与行为验证（若 AIA 规范定义）；

#### ✅ LPI / MSI 压力测试
- [ ] 使用 `KVM_SIGNAL_MSI` 构建并注入大规模 MSI 中断；
- [ ] 构造设备与 hart 的中断映射关系，模拟 MAPD/MAPTI 的行为；
- [ ] 多线程并发触发中断，统计中断注入吞吐率；
- [ ] 多 vCPU 环境下测试中断的分发、接收、EOI 等完整路径；

#### ✅ 通用框架与清理工作
- [ ] 将 `gic_init()` / `its_init()` 替换为 `aia_init()` 等 RISCV 特定初始化函数；
- [ ] 移除所有 GIC 专属常量与结构（如 `GICR_TYPER`, `IAR_SPURIOUS`, `DIR`）；
- [ ] 确保测试逻辑仅依赖 RISC-V 架构定义，避免引用 ARM 特定头文件或宏；

---

该列表旨在对齐 ARM GICv3 自测用例的测试覆盖范围，指导后续社区对 RISC-V AIA 虚拟中断支持进行测试验证的逐步推进。
