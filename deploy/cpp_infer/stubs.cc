// Stub implementations for missing complex type kernel instantiations
// These are needed because Paddle Inference was built without complex type support
#include <cstdint>

namespace phi {
namespace dtype {
struct complex_f { float real, imag; };
struct complex_d { double real, imag; };
}
}

// Provide stub implementations for missing symbols
// These will never be called in practice for OCR inference
extern "C" {
    void _ZN3phi10FullKernelINS_5dtype7complexIfEENS_10CPUContextEEE() {}
    void _ZN3phi10FullKernelINS_5dtype7complexIdEENS_10CPUContextEEE() {}
    void _ZN3phi10FullLikeKernelINS_5dtype7complexIfEENS_10CPUContextEEE() {}
    void _ZN3phi10FullLikeKernelINS_5dtype7complexIdEENS_10CPUContextEEE() {}
    void _ZN3phi11AllRawKernelINS_5dtype7complexIfEENS_10CPUContextEEE() {}
    void _ZN3phi11AllRawKernelINS_5dtype7complexIdEENS_10CPUContextEEE() {}
    void _ZN3phi11AnyRawKernelINS_5dtype7complexIfEENS_10CPUContextEEE() {}
    void _ZN3phi11AnyRawKernelINS_5dtype7complexIdEENS_10CPUContextEEE() {}
}
