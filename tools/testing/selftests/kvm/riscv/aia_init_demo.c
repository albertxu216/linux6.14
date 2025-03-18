/* SPDX-License-Identifier: GPL-2.0
 *
 * aia_init.c - AIA 初始化测试框架 for RISCV IMSIC
 *
 * 本文件主要用于测试 RISCV 下 KVM 虚拟中断 AIA（IMSIC）的地址属性配置、
 * 错误处理以及控制器初始化。
 *
 *  - vm_create_with_vcpus 保持不变
 *  - KVM_DEV_TYPE_ARM_VGIC_V3 替换为 KVM_DEV_TYPE_RISCV_AIA
 *  - __kvm_test_create_device 保持不变
 *  - gic_dev_type 替换为 KVM_DEV_TYPE_RISCV_AIA
 *  - vm_gic_destroy 替换为 Close + kvm_vm_free
 *  - vm_gic_create_with_vcpus 替换为 vm_create_with_vcpus + kvm_create_device
 *  - kvm_has_device_attr 替换为 __kvm_has_device_attr
 *  - KVM_DEV_ARM_VGIC_GRP_ADDR 替换为 KVM_DEV_RISCV_AIA_GRP_ADDR
 *  - __kvm_device_attr_set 保持不变
 *  - 对于 REDIST_REGION_ATTR_ADDR 需要自行实现（示例中用 IMSIC_REGION_ATTR_ADDR 占位）
 *  - KVM_DEV_ARM_VGIC_CTRL_INIT 替换为 KVM_DEV_RISCV_AIA_CTRL_INIT
 *
 */

#include <linux/kernel.h>
#include <sys/syscall.h>
#include <asm/kvm.h>
#include <asm/kvm_para.h>
#include "test_util.h"
#include "kvm_util.h"
#include "processor.h"

/* 如果有 AIA 相关头文件请在此包含 */
// #include "aia.h"

#define NR_VCPUS        4

/* IMSIC 寄存器访问示例宏（参考 vgic 中 REG_OFFSET 定义） */
#define REG_OFFSET(vcpu, offset)    (((uint64_t)(vcpu) << 32) | (offset))

/* 定义虚拟 IMSIC 设备结构，类似 vgic 中的 vm_gic */
struct vm_aia {
    struct kvm_vm *vm;       /* KVM 虚拟机 */
    int aia_fd;              /* AIA 设备文件描述符 */
    uint32_t aia_dev_type;   /* 设备类型，固定为 KVM_DEV_TYPE_RISCV_AIA */
};

/* dummy guest code */
static void guest_code(void)
{
    GUEST_SYNC(0);
    GUEST_SYNC(1);
    GUEST_SYNC(2);
    GUEST_DONE();
}

/* 运行 vcpu 的辅助函数 */
static int run_vcpu(struct kvm_vcpu *vcpu)
{
    /* __vcpu_run 为公共函数，riscv 下可直接使用 */
    return __vcpu_run(vcpu) ? -errno : 0;
}

/* 创建带有 VCPU 的 AIA 虚拟机：调用 vm_create_with_vcpus 和 kvm_create_device */
static struct vm_aia vm_aia_create_with_vcpus(uint32_t aia_dev_type, uint32_t nr_vcpus, struct kvm_vcpu *vcpus[])
{
    struct vm_aia a;
    a.aia_dev_type = aia_dev_type;
    a.vm = vm_create_with_vcpus(nr_vcpus, guest_code, vcpus);
    a.aia_fd = kvm_create_device(a.vm, aia_dev_type);
    return a;
}

/* 创建 barebones 形式的虚拟机 */
static struct vm_aia vm_aia_create_barebones(uint32_t aia_dev_type)
{
    struct vm_aia a;
    a.aia_dev_type = aia_dev_type;
    a.vm = vm_create_barebones();
    a.aia_fd = kvm_create_device(a.vm, aia_dev_type);
    return a;
}

/* 销毁 AIA 虚拟机 */
static void vm_aia_destroy(struct vm_aia *a)
{
    close(a->aia_fd);
    kvm_vm_free(a->vm);
}

/*
 * 定义 IMSIC 区域属性结构，类似 vgic 中的 vgic_region_attr，
 * 用于测试 IMSIC 分布器（dist）以及 IMSIC 重分发区域（IMSIC region）。
 */
struct aia_region_attr {
    uint64_t attr;      /* 属性类型，使用 KVM_DEV_RISCV_AIA_GRP_ADDR 及扩展 */
    uint64_t size;      /* 区域大小 */
    uint64_t alignment; /* 对齐要求 */
};

/* 示例定义：IMSIC 分布器区域属性 */
struct aia_region_attr aia_dist_region = {
    .attr = KVM_DEV_RISCV_AIA_GRP_ADDR, /* 使用平替后的设备属性 */
    .size = 0x10000,
    .alignment = 0x10000,
};

/*
 * IMSIC 重分发区域（或称 IMSIC CPU 接口区域）。
 * 由于原 vgic 中 REDIST_REGION_ATTR_ADDR 需要自行实现，这里用 IMSIC_REGION_ATTR_ADDR 宏占位。
 * 实际实现时，请根据 IMSIC 规格进行定义。
 */
struct aia_region_attr aia_imsic_region = {
    .attr = 0, /* 此处的属性号需要你自行定义 */
    .size = NR_VCPUS * 0x20000,
    .alignment = 0x10000,
};

/*
 * 示例 IMSIC_REGION_ATTR_ADDR 宏：
 * 用于构造 IMSIC 区域地址属性值。各字段的具体位宽和位置需要根据 IMSIC 规格确定。
 * 这里仅作占位示例：
 */
#define IMSIC_REGION_ATTR_ADDR(count, base, flags, index)  \
    (((uint64_t)(count) << 48) | ((base) & 0xFFFFFFFFFFFFULL) | ((uint64_t)(flags) << 32) | (index))

/*
 * 测试 IMSIC 区域属性配置函数：
 * 模拟测试错误情况：地址未对齐、地址超出范围等。
 */
static void subtest_imsic_region(struct vm_aia *a)
{
    int ret;
    uint64_t addr;

    /* 1. 检查分布器地址属性是否存在 */
    kvm_has_device_attr(a->aia_fd, KVM_DEV_RISCV_AIA_GRP_ADDR, aia_dist_region.attr);

    /* 2. 使用未对齐的地址测试，期望返回 EINVAL */
    addr = aia_dist_region.alignment / 0x10;
    ret = __kvm_device_attr_set(a->aia_fd, KVM_DEV_RISCV_AIA_GRP_ADDR, aia_dist_region.attr, &addr);
    TEST_ASSERT(ret && errno == EINVAL, "AIA dist base not aligned");

    /* 3. 测试地址超出范围
     * 此处假设 max_phys_size 在 kvm_util.c 中定义，这里示例赋值为 4GB。
     */
    extern uint64_t max_phys_size;
    addr = max_phys_size;
    ret = __kvm_device_attr_set(a->aia_fd, KVM_DEV_RISCV_AIA_GRP_ADDR, aia_dist_region.attr, &addr);
    TEST_ASSERT(ret && errno == E2BIG, "dist address beyond IPA limit");

    /* 4. 正确设置分布器地址，例如设置为 0 */
    addr = 0;
    kvm_device_attr_set(a->aia_fd, KVM_DEV_RISCV_AIA_GRP_ADDR, aia_dist_region.attr, &addr);

    /* 5. 测试 IMSIC 重分发区域属性：
     * 先用错误对齐的地址测试，期望返回 EINVAL
     */
    addr = IMSIC_REGION_ATTR_ADDR(NR_VCPUS, aia_imsic_region.alignment / 0x10, 0, 0);
    ret = __kvm_device_attr_set(a->aia_fd, KVM_DEV_RISCV_AIA_GRP_ADDR, aia_imsic_region.attr, &addr);
    TEST_ASSERT(ret && errno == EINVAL, "IMSIC region not aligned");

    /* 6. 正确设置 IMSIC 区域地址，例如设置为 0x100000 */
    addr = IMSIC_REGION_ATTR_ADDR(NR_VCPUS, 0x100000, 0, 0);
    kvm_device_attr_set(a->aia_fd, KVM_DEV_RISCV_AIA_GRP_ADDR, aia_imsic_region.attr, &addr);
}

/*
 * 测试流程一：
 * 先创建所有 VCPU，再配置 AIA 设备地址属性，
 * 运行 VCPU 时应检测到 IMSIC 区域配置错误（例如地址重叠）返回 -EINVAL。
 */
static void test_vcpus_then_aia(uint32_t aia_dev_type)
{
    struct kvm_vcpu *vcpus[NR_VCPUS];
    struct vm_aia a;
    int ret;

    a = vm_aia_create_with_vcpus(aia_dev_type, NR_VCPUS, vcpus);
    subtest_imsic_region(&a);

    /* 运行某个 vCPU 检测错误 */
    ret = run_vcpu(vcpus[3]);
    TEST_ASSERT(ret == -EINVAL, "IMSIC region overlap detected on VCPU run");

    vm_aia_destroy(&a);
}

/*
 * 测试流程二：
 * 先创建 AIA 设备，再添加额外 VCPU，
 * 同样配置 IMSIC 地址属性后，运行 VCPU 时应返回 -EINVAL。
 */
static void test_aia_then_vcpus(uint32_t aia_dev_type)
{
    struct kvm_vcpu *vcpus[NR_VCPUS];
    struct vm_aia a;
    int i, ret;

    a = vm_aia_create_with_vcpus(aia_dev_type, 1, vcpus);
    subtest_imsic_region(&a);

    for (i = 1; i < NR_VCPUS; ++i)
        vcpus[i] = vm_vcpu_add(a.vm, i, guest_code);

    ret = run_vcpu(vcpus[3]);
    TEST_ASSERT(ret == -EINVAL, "IMSIC region overlap detected on VCPU run");

    vm_aia_destroy(&a);
}

/*
 * 测试 AIA 控制器初始化：
 * 调用 kvm_device_attr_set 设置控制初始化属性，
 * 此处将 KVM_DEV_ARM_VGIC_CTRL_INIT 替换为 KVM_DEV_RISCV_AIA_CTRL_INIT。
 */
static void test_aia_ctrl_init(uint32_t aia_dev_type)
{
    struct kvm_vcpu *vcpus[NR_VCPUS];
    struct vm_aia a;
    int ret;

    a = vm_aia_create_with_vcpus(aia_dev_type, NR_VCPUS, vcpus);

    /* 正确配置 IMSIC 区域 */
    uint64_t addr = IMSIC_REGION_ATTR_ADDR(NR_VCPUS, 0x100000, 0, 0);
    kvm_device_attr_set(a.aia_fd, KVM_DEV_RISCV_AIA_GRP_ADDR, aia_imsic_region.attr, &addr);

    /* 初始化 AIA 设备控制器 */
    kvm_device_attr_set(a.aia_fd, KVM_DEV_RISCV_AIA_GRP_CTRL, KVM_DEV_RISCV_AIA_CTRL_INIT, NULL);

    ret = run_vcpu(vcpus[3]);
    TEST_ASSERT(!ret, "VCPU run failed after AIA initialization");

    vm_aia_destroy(&a);
}

/*
 * 主函数：依次调用各个测试用例
 */
int main(int argc, char *argv[])
{
    /* 示例设置 max_phys_size 为 4GB */
    max_phys_size = 0x100000000ULL;

    test_vcpus_then_aia(KVM_DEV_TYPE_RISCV_AIA);
    test_aia_then_vcpus(KVM_DEV_TYPE_RISCV_AIA);
    test_aia_ctrl_init(KVM_DEV_TYPE_RISCV_AIA);

    return 0;
}
