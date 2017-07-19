#pragma once

#include <stdint.h>
#include <string>
#include <string.h>
#include "RingUtil.h"

namespace queue
{

template<class MessageType, uint32_t RingSize>
class OrderRing
{
 public:
    OrderRing(void) 
    {
        CheckPowOfTwo(RingSize) ;
    }
    
    OrderRing(const std::string name) {}

    uint32_t pushAsManyAsPossible(MessageType message[], uint32_t count) ;

    inline uint32_t push(MessageType message[], uint32_t count)
    {
        uint32_t totalPushCount = 0 ;
        do{
            totalPushCount += pushAsManyAsPossible(message + totalPushCount, count - totalPushCount) ;
        } while(totalPushCount < count) ;

        return totalPushCount ;
    }

    bool push(MessageType &message) ;
    
    uint32_t push(MessageType &message, MessageType messages[], uint32_t count) ;
    
    uint32_t popAsManyAsPossible(MessageType message[], uint32_t count) ;

    inline uint32_t pop(MessageType message[], uint32_t count)
    {
        uint32_t totalPopCount = 0 ;
        do{
            totalPopCount += popAsManyAsPossible(message + totalPopCount, count - totalPopCount) ;
        } while(totalPopCount < count) ;
        
        return totalPopCount ;
    }

    bool pop(MessageType &message) ;

 private:    

    class Cursor
    {
     public:
        Cursor(void): current(0), future(0) {}
        volatile uint32_t current ;
        volatile uint32_t future ;
    } CACHE_ALIGNED ;
    
    class BitMap
    {
     public:
        BitMap(void)
        {
            memset(data, 0 , sizeof(data)) ;
        }

        inline bool get(uint32_t pos)
        {
            return data[pos] ;
        }

        inline void set(uint32_t pos)
        {
            data[pos] = 1 ;
        }

        inline void reset(uint32_t pos)
        {
            data[pos] = 0 ;
        }
         uint8_t data[RingSize] ;
    } CACHE_ALIGNED ;

    //std::string name CACHE_ALIGNED;

    Cursor cursor ;

    BitMap bitMap ;

    MessageType data[RingSize] ;
} ;


template<class MessageType, uint32_t RingSize>
uint32_t OrderRing<MessageType, RingSize>::pushAsManyAsPossible(MessageType message[], uint32_t count)
{
    uint32_t current ;
    
    uint32_t pos ;
    for(pos = 0; pos < count; ++pos)
    {
        MessageType &currentMessage = message[pos] ;
        uint32_t sequence = currentMessage.getSequence() ;
        current = cursor.current ;
        if(RingSize <= (sequence - current))
        {
            break ;
        }

        uint32_t writePos = RingUtil::GetPos<RING_MASK>(sequence) ;
        data[writePos] = currentMessage ;
        RingUtil::FlushData() ;

        bitMap.set(writePos) ;
    }
    
    return pos ;
}

template<class MessageType, uint32_t RingSize>
bool OrderRing<MessageType, RingSize>::push(MessageType &message)
{
    uint32_t sequence = message.getSequence() ;

    uint32_t current ;
    
    do{
        current = cursor.current ;
    } while(RingSize <= (sequence - current)) ;

    uint32_t writePos = RingUtil::GetPos<RING_MASK>(sequence) ;

    data[writePos] = message ;
    RingUtil::FlushData() ;

    bitMap.set(writePos) ;

    return true ;
}

template<class MessageType, uint32_t RingSize>
uint32_t OrderRing<MessageType, RingSize>::push(MessageType &message, MessageType popMessage[], uint32_t count)
{
    if(0 == count)
    {
        push(message) ;
        return 0 ;
    }

    uint32_t sequence = message.getSequence() ;    
    uint32_t current = cursor.current ;
    if(likely(sequence == current))
    {
        popMessage[0] = message ;
        cursor.future++ ;
        cursor.current++ ;
        return 1 ;
    }

    while(unlikely(RingSize <= (sequence - current)))
    {
        current = cursor.current ;
    }

    uint32_t writePos = RingUtil::GetPos<RING_MASK>(sequence) ;

    data[writePos] = message ;
    RingUtil::FlushData() ;
    
    bitMap.set(writePos) ;

    return popAsManyAsPossible(popMessage, count) ;
}

template<class MessageType, uint32_t RingSize>
uint32_t OrderRing<MessageType, RingSize>::popAsManyAsPossible(MessageType message[], uint32_t count)
{
    uint32_t future = cursor.future ;
    uint32_t current = cursor.current ;
    
    if(unlikely(current != future))
    {
        return 0 ;
    }

    if(unlikely(RingSize < count))
    {
        count = RingSize ;
    }

    for(; future < (current + count); ++future)
    {
        uint32_t readPos = RingUtil::GetPos<RING_MASK>(future) ;
        if(!bitMap.get(readPos))
        {
            break ;
        }
    }

    uint32_t storedCount = future - current ;
    if(unlikely(0 == storedCount))
    {
        return 0 ;
    }

    if(unlikely(!RingUtil::CAS(&cursor.future, current, future)))
    {
        return 0 ;
    }

    uint32_t readPos = RingUtil::GetPos<RING_MASK>(current) ;
    if(likely(RingSize > readPos + storedCount))
    {
        uint32_t pos ;
        for( pos = 0; pos < (storedCount & (~(uint32_t)0x03)); pos += 4, readPos += 4)
        {
            message[pos] = data[readPos] ;
            message[pos + 1] = data[readPos + 1] ;
            message[pos + 2] = data[readPos + 2] ;
            message[pos + 3] = data[readPos + 3] ;
            bitMap.reset(readPos) ;
            bitMap.reset(readPos + 1) ;
            bitMap.reset(readPos + 2) ;
            bitMap.reset(readPos + 3) ;
        }
        switch(storedCount & (uint32_t)0x03)
        {
            case 3:
            {
                message[pos++] = data[readPos] ;
                bitMap.reset(readPos++) ;
            }
            case 2:
            {
                message[pos++] = data[readPos] ;
                bitMap.reset(readPos++) ;
            }
            case 1:
            {
                message[pos++] = data[readPos] ;
                bitMap.reset(readPos++) ;
            }
        }
    }else
    {
        uint32_t pos ;
        for( pos = 0; readPos < RingSize; readPos++)
        {
            message[pos++] = data[readPos] ;
            bitMap.reset(readPos) ;
        }
        for( readPos = 0; pos < storedCount; readPos++)
        {
            message[pos++] = data[readPos] ;
            bitMap.reset(readPos) ;
        }
    }

    RingUtil::FlushData() ;
    cursor.current = future ;

    return storedCount ;
}


template<class MessageType, uint32_t RingSize>
bool OrderRing<MessageType, RingSize>::pop(MessageType &message)
{
    uint32_t current ;
    uint32_t future ;
    uint32_t readPos ;

    for( ; ; )
    {
        future = cursor.future ;
        current = cursor.current ;

        if(likely(current == future))
        {
            readPos = RingUtil::GetPos<RING_MASK>(current) ;
            if(bitMap.get(readPos))
            {
                if(likely(RingUtil::CAS(&cursor.future, future, future + 1)))
                {
                    break ;
                }
            }
        }
    }

    message = data[readPos] ;
    bitMap.reset(readPos) ;
    RingUtil::FlushData() ;

    cursor.current = future + 1 ;

    return true ;
}

}