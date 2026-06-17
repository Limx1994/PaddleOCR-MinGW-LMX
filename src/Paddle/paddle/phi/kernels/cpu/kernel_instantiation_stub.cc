// Copyright (c) 2024 PaddlePaddle Authors. All Rights Reserved.
//
// Explicit template instantiation stub for MinGW GCC.
// GCC's IPA passes inline template instantiations and convert them to local
// symbols (.isra.0), causing undefined reference errors during linking.
// This file forces global instantiation of affected templates.

#include "paddle/phi/kernels/impl/isfinite_kernel_impl.h"
#include "paddle/phi/backends/cpu/cpu_context.h"

namespace phi {

// IsinfKernel - explicit instantiation
template void IsinfKernel<float, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);
template void IsinfKernel<double, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);
template void IsinfKernel<int, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);
template void IsinfKernel<int64_t, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);
template void IsinfKernel<float16, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);
template void IsinfKernel<bfloat16, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);

// IsnanKernel - explicit instantiation
template void IsnanKernel<float, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);
template void IsnanKernel<double, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);
template void IsnanKernel<int, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);
template void IsnanKernel<int64_t, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);
template void IsnanKernel<float16, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);
template void IsnanKernel<bfloat16, CPUContext>(const CPUContext&, const DenseTensor&, DenseTensor*);

}  // namespace phi
