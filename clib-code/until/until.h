#ifndef _UNTIL_H_
#define _UNTIL_H_

#define min(a, b) ((a) < (b) ? (a) : (b))                                                                      // 比较小值
#define max(a, b) ((a) > (b) ? (a) : (b))                                                                      // 比较大值                                                                     // 绝对值
#define constrain(x, a, b) max(a, min(b, x))                                                                   // 限制范围 x 限幅 [a, b]
#define linear_map(x, x1, x2, y1, y2) constrain(((y1) + ((x) - (x1)) * ((y2) - (y1)) / ((x2) - (x1))), y1, y2) // 线性映射 x 从 [x1, x2] 到 [y1, y2] x2 应该大于 x1, y2 应该大于 y1

#define BIT(n) (1U << (n))                                                                                     // 位掩码 第 n 位为 1
#define SET_BIT(x, n) ((x) |= BIT(n))                                                                          // 置位 x 的第 n 位
#define CLEAR_BIT(x, n) ((x) &= ~BIT(n))                                                                       // 清除 x 的第 n 位
#define TOGGLE_BIT(x, n) ((x) ^= BIT(n))                                                                       // 翻转 x 的第 n 位
#define READ_BIT(x, n) (((x) >> (n)) & 1U)                                                                     // 读取 x 的第 n 位
#define REG_SET(x, cmark, smark) ((x) = ((x) & ~(cmark)) | (smark))                                            // 按位设置 x 的指定位

#define OFFSETOF(type, member) ((unsigned long)&(((type *)0)->member))                                         // 成员相对结构体起始的字节偏移
#define CONTAINER_OF(ptr, type, member) ((type *)((char *)(ptr) - OFFSETOF(type, member)))                     // 由成员指针获取宿主结构体指针

#define _STRINGIFY(x) #x                                                                                       // 字符串化（内部辅助）
#define STRINGIFY(x) _STRINGIFY(x)                                                                             // 转字符串字面量
#define _CONCAT(a, b) a##b                                                                                     // 标识符拼接（内部辅助）
#define CONCAT(a, b) _CONCAT(a, b)                                                                             // 标识符拼接

#ifndef ASSERT
#define ASSERT(expr)                                                                                           \
    do                                                                                                          \
    {                                                                                                           \
        if (!(expr))                                                                                            \
        {                                                                                                       \
            while (1)                                                                                           \
            {                                                                                                   \
            }                                                                                                   \
        }                                                                                                       \
    } while (0)
#endif

// 编译器属性（自动识别编译器，未识别的编译器下属性宏会退化为空以避免报错）
#if defined(__GNUC__) || defined(__clang__)
#define WEAK         __attribute__((weak))
#define UNUSED       __attribute__((unused))
#define PACKED       __attribute__((packed))
#define INLINE       static inline __attribute__((always_inline))
#define NORETURN     __attribute__((noreturn))
#define DEPRECATED   __attribute__((deprecated))
#define ALIGNED(n)   __attribute__((aligned(n)))
#define SECTION(s)   __attribute__((section(s)))
#define LIKELY(x)    __builtin_expect(!!(x), 1)
#define UNLIKELY(x)  __builtin_expect(!!(x), 0)
#elif defined(__CC_ARM)
#define WEAK         __attribute__((weak))
#define UNUSED       __attribute__((unused))
#define PACKED       __attribute__((packed))
#define INLINE       static __forceinline
#define NORETURN     __attribute__((noreturn))
#define DEPRECATED   __attribute__((deprecated))
#define ALIGNED(n)   __attribute__((aligned(n)))
#define SECTION(s)   __attribute__((section(s)))
#define LIKELY(x)    (x)
#define UNLIKELY(x)  (x)
#elif defined(__ICCARM__)
#define WEAK         __weak
#define UNUSED
#define PACKED       __packed
#define INLINE       static inline
#define NORETURN     __noreturn
#define DEPRECATED
#define ALIGNED(n)   _Pragma(STRINGIFY(data_alignment=n))
#define SECTION(s)
#define LIKELY(x)    (x)
#define UNLIKELY(x)  (x)
#elif defined(_MSC_VER)
#define WEAK         __declspec(selectany)
#define UNUSED
#define PACKED
#define INLINE       __forceinline
#define NORETURN     __declspec(noreturn)
#define DEPRECATED   __declspec(deprecated)
#define ALIGNED(n)   __declspec(align(n))
#define SECTION(s)   __declspec(allocate(s))
#define LIKELY(x)    (x)
#define UNLIKELY(x)  (x)
#else
#warning "未识别的编译器，部分属性宏将展开为空"
#define WEAK
#define UNUSED
#define PACKED
#define INLINE       static inline
#define NORETURN
#define DEPRECATED
#define ALIGNED(n)
#define SECTION(s)
#define LIKELY(x)    (x)
#define UNLIKELY(x)  (x)
#endif
#endif
