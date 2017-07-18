#pragma once

#include <stdint.h>
#include <string>

namespace queue
{

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x)	__builtin_expect(!!(x), 0)
#endif

#ifndef CACHE_ALIGNED
#define CACHE_ALIGNED __attribute__((__aligned__(CACHE_LINE_SIZE)))
#endif

#define RING_MASK (RingSize - 1)

class RingUtil
{
 public:
    static inline bool CAS(volatile uint32_t *dst, uint32_t exp, uint32_t src)
    {
        uint8_t success ;
        asm volatile(
            "lock ;"
            "cmpxchgl %[src], %[dst] ;"
            "sete %[success] ;"
            : [success] "=a" (success), [dst] "=m" (*dst)
            : [src] "r" (src), "a" (exp), "m" (*dst)
            : "memory") ;

        return success ;
    }

    template<uint32_t RingMask>
    static inline uint32_t GetPos(uint32_t pos)
    {
        return RingMask & pos ;
    }
    
    static inline void FlushData(void)
    {
        asm volatile("sfence"::: "memory") ;
    }
    
 private:    
} ;

}