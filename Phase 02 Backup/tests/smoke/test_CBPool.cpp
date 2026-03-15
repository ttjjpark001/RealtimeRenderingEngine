#include <gtest/gtest.h>
#include "RHI/D3D12/D3D12CBPool.h"
#include "RHI/D3D12/D3D12DescriptorHeap.h"
#include <dxgi1_4.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{

// Create a WARP device for GPU-less testing
ComPtr<ID3D12Device> CreateWARPDevice()
{
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return nullptr;

    ComPtr<IDXGIAdapter> warpAdapter;
    if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter))))
        return nullptr;

    ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device))))
        return nullptr;

    return device;
}

} // anonymous namespace

// ============================================================================
// D3D12CBPool Tests
// ============================================================================

class CBPoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_device = CreateWARPDevice();
        if (!m_device)
            GTEST_SKIP() << "WARP device not available";
    }

    ComPtr<ID3D12Device> m_device;
};

TEST_F(CBPoolTest, Initialize_Succeeds)
{
    RRE::D3D12CBPool pool;
    EXPECT_TRUE(pool.Initialize(m_device.Get()));
    pool.Shutdown();
}

TEST_F(CBPoolTest, Initialize_CustomSize)
{
    RRE::D3D12CBPool pool;
    constexpr uint32_t customSize = 1024 * 1024; // 1MB
    EXPECT_TRUE(pool.Initialize(m_device.Get(), customSize));
    EXPECT_EQ(pool.GetTotalSize(), customSize);
    pool.Shutdown();
}

TEST_F(CBPoolTest, Allocate_Returns256ByteAligned)
{
    RRE::D3D12CBPool pool;
    ASSERT_TRUE(pool.Initialize(m_device.Get()));

    pool.ResetFrame(0);

    // Allocate various sizes, verify 256-byte alignment
    auto alloc1 = pool.Allocate(100);
    EXPECT_NE(alloc1.cpuPtr, nullptr);
    EXPECT_EQ(alloc1.gpuAddress % 256, 0u);

    auto alloc2 = pool.Allocate(200);
    EXPECT_NE(alloc2.cpuPtr, nullptr);
    EXPECT_EQ(alloc2.gpuAddress % 256, 0u);

    auto alloc3 = pool.Allocate(256);
    EXPECT_NE(alloc3.cpuPtr, nullptr);
    EXPECT_EQ(alloc3.gpuAddress % 256, 0u);

    // Allocations should not overlap
    EXPECT_NE(alloc1.cpuPtr, alloc2.cpuPtr);
    EXPECT_NE(alloc2.cpuPtr, alloc3.cpuPtr);

    pool.Shutdown();
}

TEST_F(CBPoolTest, Allocate_ConsecutiveSlots)
{
    RRE::D3D12CBPool pool;
    ASSERT_TRUE(pool.Initialize(m_device.Get()));

    pool.ResetFrame(0);

    auto alloc1 = pool.Allocate(256);
    auto alloc2 = pool.Allocate(256);

    // Second allocation should be exactly 256 bytes after first
    EXPECT_EQ(alloc2.gpuAddress - alloc1.gpuAddress, 256u);
    EXPECT_EQ(alloc2.cpuPtr - alloc1.cpuPtr, 256);

    pool.Shutdown();
}

TEST_F(CBPoolTest, ResetFrame_AllowsReallocation)
{
    RRE::D3D12CBPool pool;
    ASSERT_TRUE(pool.Initialize(m_device.Get()));

    // Frame 0: allocate
    pool.ResetFrame(0);
    auto alloc0 = pool.Allocate(256);
    EXPECT_NE(alloc0.cpuPtr, nullptr);

    // Frame 1: allocate from second half
    pool.ResetFrame(1);
    auto alloc1 = pool.Allocate(256);
    EXPECT_NE(alloc1.cpuPtr, nullptr);

    // Frame 0 again: should be able to allocate from first half again
    pool.ResetFrame(0);
    auto alloc0b = pool.Allocate(256);
    EXPECT_NE(alloc0b.cpuPtr, nullptr);

    // alloc0b should reuse the same address as alloc0
    EXPECT_EQ(alloc0b.gpuAddress, alloc0.gpuAddress);

    pool.Shutdown();
}

TEST_F(CBPoolTest, DoubleBuffering_SeparateRegions)
{
    RRE::D3D12CBPool pool;
    constexpr uint32_t totalSize = 1024 * 1024; // 1MB
    ASSERT_TRUE(pool.Initialize(m_device.Get(), totalSize));

    // Frame 0: first half
    pool.ResetFrame(0);
    auto alloc0 = pool.Allocate(256);

    // Frame 1: second half
    pool.ResetFrame(1);
    auto alloc1 = pool.Allocate(256);

    // They should be in different halves (512KB apart)
    uint64_t diff = alloc1.gpuAddress - alloc0.gpuAddress;
    EXPECT_EQ(diff, totalSize / 2);

    pool.Shutdown();
}

TEST_F(CBPoolTest, WriteAndRead_Data)
{
    RRE::D3D12CBPool pool;
    ASSERT_TRUE(pool.Initialize(m_device.Get()));

    pool.ResetFrame(0);
    auto alloc = pool.Allocate(256);
    ASSERT_NE(alloc.cpuPtr, nullptr);

    // Write test data through CPU pointer
    float testData[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    memcpy(alloc.cpuPtr, testData, sizeof(testData));

    // Read back through CPU pointer
    float readBack[4];
    memcpy(readBack, alloc.cpuPtr, sizeof(readBack));
    EXPECT_FLOAT_EQ(readBack[0], 1.0f);
    EXPECT_FLOAT_EQ(readBack[1], 2.0f);
    EXPECT_FLOAT_EQ(readBack[2], 3.0f);
    EXPECT_FLOAT_EQ(readBack[3], 4.0f);

    pool.Shutdown();
}

// ============================================================================
// DescriptorHeap Split Tests
// ============================================================================

class DescriptorHeapSplitTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_device = CreateWARPDevice();
        if (!m_device)
            GTEST_SKIP() << "WARP device not available";
    }

    ComPtr<ID3D12Device> m_device;
};

TEST_F(DescriptorHeapSplitTest, Initialize_Split_Succeeds)
{
    RRE::D3D12DescriptorHeap heap;
    EXPECT_TRUE(heap.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        128, 256, true));
}

TEST_F(DescriptorHeapSplitTest, AllocatePersistent_ReturnsValidHandle)
{
    RRE::D3D12DescriptorHeap heap;
    ASSERT_TRUE(heap.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        128, 256, true));

    auto handle1 = heap.AllocatePersistent();
    auto handle2 = heap.AllocatePersistent();

    EXPECT_NE(handle1.ptr, 0u);
    EXPECT_NE(handle2.ptr, 0u);
    EXPECT_NE(handle1.ptr, handle2.ptr);
}

TEST_F(DescriptorHeapSplitTest, AllocateTransient_ReturnsValidHandle)
{
    RRE::D3D12DescriptorHeap heap;
    ASSERT_TRUE(heap.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        128, 256, true));

    auto handle1 = heap.AllocateTransient();
    auto handle2 = heap.AllocateTransient();

    EXPECT_NE(handle1.ptr, 0u);
    EXPECT_NE(handle2.ptr, 0u);
    EXPECT_NE(handle1.ptr, handle2.ptr);
}

TEST_F(DescriptorHeapSplitTest, ResetTransient_AllowsReallocation)
{
    RRE::D3D12DescriptorHeap heap;
    ASSERT_TRUE(heap.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        128, 256, true));

    auto handle1 = heap.AllocateTransient();

    heap.ResetTransient();

    auto handle2 = heap.AllocateTransient();

    // After reset, should get the same handle back
    EXPECT_EQ(handle1.ptr, handle2.ptr);
}

TEST_F(DescriptorHeapSplitTest, PersistentAndTransient_DontOverlap)
{
    RRE::D3D12DescriptorHeap heap;
    constexpr uint32_t persistentCount = 4;
    constexpr uint32_t transientCount = 8;
    ASSERT_TRUE(heap.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        persistentCount, transientCount, true));

    // Allocate all persistent
    D3D12_CPU_DESCRIPTOR_HANDLE persistentHandles[persistentCount];
    for (uint32_t i = 0; i < persistentCount; i++)
        persistentHandles[i] = heap.AllocatePersistent();

    // Allocate some transient
    D3D12_CPU_DESCRIPTOR_HANDLE transientHandles[4];
    for (uint32_t i = 0; i < 4; i++)
        transientHandles[i] = heap.AllocateTransient();

    // All handles should be unique
    for (uint32_t i = 0; i < persistentCount; i++)
        for (uint32_t j = 0; j < 4; j++)
            EXPECT_NE(persistentHandles[i].ptr, transientHandles[j].ptr);
}

TEST_F(DescriptorHeapSplitTest, GetGPUHandleForCPU_Valid)
{
    RRE::D3D12DescriptorHeap heap;
    ASSERT_TRUE(heap.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        128, 256, true));

    auto cpuHandle = heap.AllocatePersistent();
    auto gpuHandle = heap.GetGPUHandleForCPU(cpuHandle);

    EXPECT_NE(gpuHandle.ptr, 0u);
}

TEST_F(DescriptorHeapSplitTest, LegacyAllocate_DelegatesToTransient)
{
    RRE::D3D12DescriptorHeap heap;
    ASSERT_TRUE(heap.Initialize(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        4, 8, true));

    // Legacy Allocate should use transient region
    auto handle = heap.Allocate();
    EXPECT_NE(handle.ptr, 0u);

    // Legacy Reset should reset transient
    heap.Reset();
    auto handle2 = heap.Allocate();
    EXPECT_EQ(handle.ptr, handle2.ptr);
}
