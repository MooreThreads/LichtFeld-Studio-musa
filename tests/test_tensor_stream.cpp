/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <musa_runtime.h>
#include <gtest/gtest.h>
#include <numeric>
#include <vector>

#include "core/tensor.hpp"
#include "core/tensor/internal/cuda_stream_context.hpp"

using namespace lfs::core;

class TensorStreamTest : public ::testing::Test {
protected:
    void SetUp() override {
        musaSetDevice(0);
    }
};

TEST_F(TensorStreamTest, DefaultStreamIsNullptr) {
    auto t = Tensor::empty({4, 4}, Device::CUDA);
    EXPECT_EQ(t.stream(), nullptr);
}

TEST_F(TensorStreamTest, FactoryPicksUpThreadLocalStream) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreate(&stream), musaSuccess);

    {
        CUDAStreamGuard guard(stream);
        auto t = Tensor::empty({8, 8}, Device::CUDA);
        EXPECT_EQ(t.stream(), stream);

        auto z = Tensor::zeros({4}, Device::CUDA);
        EXPECT_EQ(z.stream(), stream);

        auto o = Tensor::ones({4}, Device::CUDA);
        EXPECT_EQ(o.stream(), stream);

        auto f = Tensor::full({4}, 3.14f, Device::CUDA);
        EXPECT_EQ(f.stream(), stream);

        auto r = Tensor::rand({4}, Device::CUDA);
        EXPECT_EQ(r.stream(), stream);

        auto rn = Tensor::randn({4}, Device::CUDA);
        EXPECT_EQ(rn.stream(), stream);

        auto a = Tensor::arange(0, 10, 1);
        EXPECT_EQ(a.stream(), stream);
    }

    // After guard, back to default
    auto t2 = Tensor::empty({4, 4}, Device::CUDA);
    EXPECT_EQ(t2.stream(), nullptr);

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, ViewInheritsStreamReshape) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreate(&stream), musaSuccess);

    Tensor t;
    {
        CUDAStreamGuard guard(stream);
        t = Tensor::empty({4, 4}, Device::CUDA);
    }
    ASSERT_EQ(t.stream(), stream);

    auto reshaped = t.reshape({16});
    EXPECT_EQ(reshaped.stream(), stream);

    auto unsqueezed = t.unsqueeze(0);
    EXPECT_EQ(unsqueezed.stream(), stream);

    auto squeezed = unsqueezed.squeeze(0);
    EXPECT_EQ(squeezed.stream(), stream);

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, ViewInheritsStreamSlice) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreate(&stream), musaSuccess);

    Tensor t;
    {
        CUDAStreamGuard guard(stream);
        t = Tensor::full({10, 3}, 1.0f, Device::CUDA);
    }
    ASSERT_EQ(t.stream(), stream);

    auto sliced = t.slice(0, 0, 5);
    EXPECT_EQ(sliced.stream(), stream);

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, ViewInheritsStreamPermute) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreate(&stream), musaSuccess);

    Tensor t;
    {
        CUDAStreamGuard guard(stream);
        t = Tensor::empty({2, 3, 4}, Device::CUDA);
    }
    ASSERT_EQ(t.stream(), stream);

    auto permuted = t.permute({2, 0, 1});
    EXPECT_EQ(permuted.stream(), stream);

    auto transposed = t.transpose(0, 1);
    EXPECT_EQ(transposed.stream(), stream);

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, OperationsProduceResultWithCorrectStream) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreate(&stream), musaSuccess);

    Tensor a, b;
    {
        CUDAStreamGuard guard(stream);
        a = Tensor::ones({64}, Device::CUDA);
        b = Tensor::ones({64}, Device::CUDA);
    }

    // Binary ops create result via empty(), which stamps getCurrentCUDAStream()
    // Since no guard is active now, result gets nullptr (default stream)
    auto c = a + b;
    EXPECT_EQ(c.stream(), nullptr);

    // With guard active, result gets the guarded stream
    {
        CUDAStreamGuard guard(stream);
        auto d = a + b;
        EXPECT_EQ(d.stream(), stream);
    }

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, EmptyLikeInheritsStream) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreate(&stream), musaSuccess);

    Tensor t;
    {
        CUDAStreamGuard guard(stream);
        t = Tensor::ones({32}, Device::CUDA);
    }
    ASSERT_EQ(t.stream(), stream);

    auto like = Tensor::empty_like(t);
    EXPECT_EQ(like.stream(), stream);

    auto flike = Tensor::full_like(t, 42.0f);
    EXPECT_EQ(flike.stream(), stream);

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, StreamOrderingCorrectness) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreate(&stream), musaSuccess);

    Tensor result;
    {
        CUDAStreamGuard guard(stream);
        auto t = Tensor::full({1024}, 1.0f, Device::CUDA);
        auto added = t + t;          // 2.0
        auto mulled = added * added; // 4.0
        result = mulled;
    }

    musaStreamSynchronize(stream);

    auto cpu = result.to(Device::CPU);
    auto vals = cpu.to_vector();
    ASSERT_GT(vals.size(), 0u);
    EXPECT_NEAR(vals[0], 4.0f, 1e-5f);
    EXPECT_NEAR(vals.back(), 4.0f, 1e-5f);

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, CrossStreamOrderingWithEventWait) {
    musaStream_t producer, consumer;
    musaEvent_t ready;
    ASSERT_EQ(musaStreamCreate(&producer), musaSuccess);
    ASSERT_EQ(musaStreamCreate(&consumer), musaSuccess);
    ASSERT_EQ(musaEventCreateWithFlags(&ready, musaEventDisableTiming), musaSuccess);

    Tensor produced, consumed;
    {
        CUDAStreamGuard guard(producer);
        auto base = Tensor::full({2048}, 2.0f, Device::CUDA);
        produced = base.mul(3.0f); // 6.0
    }

    ASSERT_EQ(musaEventRecord(ready, producer), musaSuccess);
    ASSERT_EQ(musaStreamWaitEvent(consumer, ready, 0), musaSuccess);

    {
        CUDAStreamGuard guard(consumer);
        consumed = produced.add(1.0f); // 7.0
    }

    ASSERT_EQ(musaStreamSynchronize(consumer), musaSuccess);
    auto vals = consumed.to(Device::CPU).to_vector();
    ASSERT_EQ(vals.size(), 2048u);
    EXPECT_NEAR(vals.front(), 7.0f, 1e-5f);
    EXPECT_NEAR(vals.back(), 7.0f, 1e-5f);

    musaEventDestroy(ready);
    musaStreamDestroy(consumer);
    musaStreamDestroy(producer);
}

TEST_F(TensorStreamTest, SetStreamManual) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreate(&stream), musaSuccess);

    auto t = Tensor::empty({4}, Device::CUDA);
    EXPECT_EQ(t.stream(), nullptr);

    t.set_stream(stream);
    EXPECT_EQ(t.stream(), stream);

    t.set_stream(nullptr);
    EXPECT_EQ(t.stream(), nullptr);

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, GuardRestoresPreviousStream) {
    musaStream_t s1, s2;
    ASSERT_EQ(musaStreamCreate(&s1), musaSuccess);
    ASSERT_EQ(musaStreamCreate(&s2), musaSuccess);

    {
        CUDAStreamGuard g1(s1);
        EXPECT_EQ(getCurrentCUDAStream(), s1);

        {
            CUDAStreamGuard g2(s2);
            EXPECT_EQ(getCurrentCUDAStream(), s2);
        }

        EXPECT_EQ(getCurrentCUDAStream(), s1);
    }

    EXPECT_EQ(getCurrentCUDAStream(), nullptr);

    musaStreamDestroy(s1);
    musaStreamDestroy(s2);
}

TEST_F(TensorStreamTest, InplaceOpsUseOwnStream) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreate(&stream), musaSuccess);

    Tensor t;
    {
        CUDAStreamGuard guard(stream);
        t = Tensor::ones({256}, Device::CUDA);
    }
    ASSERT_EQ(t.stream(), stream);

    // In-place scalar op should use the tensor's stream
    t = t + Tensor::full({256}, 1.0f, Device::CUDA);
    // Result gets default stream since no guard active, so re-stamp
    t.set_stream(stream);

    // In-place binary op with guard
    {
        CUDAStreamGuard guard(stream);
        auto other = Tensor::ones({256}, Device::CUDA);
        t = t + other;
        EXPECT_EQ(t.stream(), stream);
    }

    musaStreamSynchronize(stream);

    auto vals = t.to(Device::CPU).to_vector();
    ASSERT_GT(vals.size(), 0u);
    EXPECT_NEAR(vals[0], 3.0f, 1e-5f);

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, MaskedFillRespectsTensorStreamWithoutGuard) {
    musaStream_t stream;
    musaEvent_t gate;
    ASSERT_EQ(musaStreamCreateWithFlags(&stream, musaStreamNonBlocking), musaSuccess);
    ASSERT_EQ(musaEventCreateWithFlags(&gate, musaEventDisableTiming), musaSuccess);

    Tensor t;
    Tensor mask;
    {
        CUDAStreamGuard guard(stream);
        t = Tensor::zeros({1 << 16}, Device::CUDA);
        mask = Tensor::ones({1 << 16}, Device::CUDA, DataType::Bool);
        ASSERT_EQ(musaStreamWaitEvent(stream, gate, 0), musaSuccess);
        t.fill_(1.0f, stream);
    }

    // Must enqueue on tensor stream (not implicit default stream).
    t.masked_fill_(mask, 2.0f);

    ASSERT_EQ(musaEventRecord(gate, nullptr), musaSuccess);
    ASSERT_EQ(musaStreamSynchronize(stream), musaSuccess);

    auto vals = t.to(Device::CPU).to_vector();
    ASSERT_EQ(vals.size(), static_cast<size_t>(1 << 16));
    EXPECT_NEAR(vals.front(), 2.0f, 1e-5f);
    EXPECT_NEAR(vals.back(), 2.0f, 1e-5f);

    musaEventDestroy(gate);
    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, GatherRespectsTensorStreamWithoutGuard) {
    musaStream_t stream;
    musaEvent_t gate;
    ASSERT_EQ(musaStreamCreateWithFlags(&stream, musaStreamNonBlocking), musaSuccess);
    ASSERT_EQ(musaEventCreateWithFlags(&gate, musaEventDisableTiming), musaSuccess);

    constexpr size_t count = 4096;
    std::vector<int> host_indices(count);
    std::iota(host_indices.begin(), host_indices.end(), 0);

    Tensor src;
    Tensor idx;
    {
        CUDAStreamGuard guard(stream);
        src = Tensor::zeros({count}, Device::CUDA);
        idx = Tensor::from_vector(host_indices, {count}, Device::CUDA);
        ASSERT_EQ(musaStreamWaitEvent(stream, gate, 0), musaSuccess);
        src.fill_(3.0f, stream);
    }

    // Must read from src on src.stream() to preserve producer ordering.
    auto gathered = src.gather(0, idx);

    ASSERT_EQ(musaEventRecord(gate, nullptr), musaSuccess);
    ASSERT_EQ(musaStreamSynchronize(stream), musaSuccess);

    auto vals = gathered.to(Device::CPU).to_vector();
    ASSERT_EQ(vals.size(), count);
    EXPECT_NEAR(vals.front(), 3.0f, 1e-5f);
    EXPECT_NEAR(vals.back(), 3.0f, 1e-5f);

    musaEventDestroy(gate);
    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, ToDeviceNonContiguousCpuToCudaRespectsExplicitStream) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreateWithFlags(&stream, musaStreamNonBlocking), musaSuccess);

    std::vector<float> host(20);
    std::iota(host.begin(), host.end(), 1.0f);

    auto cpu = Tensor::from_vector(host, {4, 5}, Device::CPU);
    auto view = cpu.slice(0, 0, 3).slice(1, 0, 4);
    ASSERT_FALSE(view.is_contiguous());
    EXPECT_EQ(view.stream(), nullptr);

    auto gpu = view.to(Device::CUDA, stream);
    EXPECT_EQ(gpu.stream(), stream);
    EXPECT_EQ(view.stream(), stream);

    ASSERT_EQ(musaStreamSynchronize(stream), musaSuccess);
    auto back = gpu.to(Device::CPU).to_vector();
    ASSERT_EQ(back.size(), 12u);
    EXPECT_FLOAT_EQ(back[0], 1.0f);
    EXPECT_FLOAT_EQ(back[4], 6.0f);
    EXPECT_FLOAT_EQ(back[11], 14.0f);

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, ToDeviceCudaToCpuRespectsExplicitStreamMetadata) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreateWithFlags(&stream, musaStreamNonBlocking), musaSuccess);

    auto gpu = Tensor::full({1024}, 2.0f, Device::CUDA);
    auto cpu = gpu.to(Device::CPU, stream);
    EXPECT_EQ(cpu.stream(), stream);

    ASSERT_EQ(musaStreamSynchronize(stream), musaSuccess);
    auto vals = cpu.to_vector();
    ASSERT_EQ(vals.size(), 1024u);
    EXPECT_NEAR(vals.front(), 2.0f, 1e-5f);
    EXPECT_NEAR(vals.back(), 2.0f, 1e-5f);

    musaStreamDestroy(stream);
}

TEST_F(TensorStreamTest, DtypeConversionLaunchesOnCurrentResultStream) {
    musaStream_t stream;
    ASSERT_EQ(musaStreamCreateWithFlags(&stream, musaStreamNonBlocking), musaSuccess);

    constexpr size_t kNumel = 1u << 20;
    auto src = Tensor::arange(0.0f, static_cast<float>(kNumel), 1.0f);
    ASSERT_EQ(musaDeviceSynchronize(), musaSuccess);

    // Keep default stream busy. Conversion must still execute correctly when launched on a non-default stream.
    auto busy = Tensor::ones({1u << 25}, Device::CUDA);
    for (int i = 0; i < 6; ++i) {
        busy = busy.mul(1.0001f).add(0.0001f);
    }

    Tensor converted;
    {
        CUDAStreamGuard guard(stream);
        converted = src.to(DataType::Int32);
        EXPECT_EQ(converted.stream(), stream);
    }

    auto cpu = converted.to(Device::CPU, stream);
    ASSERT_EQ(musaStreamSynchronize(stream), musaSuccess);
    const int* cpu_ptr = cpu.ptr<int>();
    ASSERT_NE(cpu_ptr, nullptr);
    EXPECT_EQ(cpu_ptr[0], 0);
    EXPECT_EQ(cpu_ptr[kNumel / 2], static_cast<int>(kNumel / 2));
    EXPECT_EQ(cpu_ptr[kNumel - 1], static_cast<int>(kNumel - 1));

    ASSERT_EQ(musaDeviceSynchronize(), musaSuccess);
    musaStreamDestroy(stream);
}
