#pragma once

#include <stdint.h>
#include <string>
#include "RingUtil.h"

namespace queue
{

template<class MessageType, uint32_t RingSize>
class Ring
{
 public:
 
    Ring(void)
    {
        CheckPowOfTwo(RingSize) ;
    }
    
    Ring(const std::string name) {}

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
    
    void UpdateCurrentCursor(Cursor &cursor, uint32_t expectedValue, uint32_t newValue)
    {
        //while(!RingUtil::CAS(&cursor.current, expectedValue, newValue)) {}        
        while(unlikely(cursor.current != expectedValue)) {}
        cursor.current = newValue ;        
    }

    //std::string name CACHE_ALIGNED;
 
    Cursor producer ;

    Cursor consumer ;

    MessageType data[RingSize] ;
} ;


template<class MessageType, uint32_t RingSize>
uint32_t Ring<MessageType, RingSize>::pushAsManyAsPossible(MessageType message[], uint32_t count)
{
    uint32_t current ;
    uint32_t future ;
    uint32_t emptyCount ;
    for( ; ; )
    {
        future = producer.future ;
        current = consumer.current ;
        
        emptyCount = (RING_MASK + current) - future ;
        if(unlikely(0 == emptyCount))
        {
            return 0 ;
        }

        if(unlikely(count < emptyCount))
        {
            emptyCount = count ;
        }

        if(likely(RingUtil::CAS(&producer.future, future, future + emptyCount)))
        {
            break ;
        }
    }

    uint32_t writePos = RingUtil::GetPos<RING_MASK>(future) ;
    if(likely(RingSize > writePos + emptyCount))
    {
        uint32_t pos ;
        for( pos = 0; pos < (emptyCount & (~(uint32_t)0x03)); pos += 4, writePos += 4)
        {
            data[writePos] = message[pos] ;
            data[writePos + 1] = message[pos + 1] ;
            data[writePos + 2] = message[pos + 2] ;
            data[writePos + 3] = message[pos + 3] ;
        }
        switch(emptyCount & (uint32_t)0x03)
        {
            case 3: { data[writePos++] = message[pos++] ; }                
            case 2: { data[writePos++] = message[pos++] ; }
            case 1: { data[writePos++] = message[pos++] ; }
        }
    }else
    {
        uint32_t pos ;
        for( pos = 0; writePos < RingSize; )
        {
            data[writePos++] = message[pos++] ;
        }

        for( writePos = 0; pos < emptyCount; )
        {
            data[writePos++] = message[pos++] ;
        }        
    }

    RingUtil::FlushData() ;

    UpdateCurrentCursor(producer, future, future + emptyCount) ;

    return emptyCount ;
}

template<class MessageType, uint32_t RingSize>
bool Ring<MessageType, RingSize>::push(MessageType &message)
{
    uint32_t current ;
    uint32_t future ;

    for( ; ; )
    {
        future = producer.future ;
        current = consumer.current ;
        
        if(unlikely(0 == (RING_MASK + current) - future))
        {
            continue ;
        }
        
        if(likely(RingUtil::CAS(&producer.future, future, future + 1)))
        {            
            break ;
        }
    }
    
    uint32_t writePos = RingUtil::GetPos<RING_MASK>(future) ;
    data[writePos] = message ;

    RingUtil::FlushData() ;
        
    UpdateCurrentCursor(producer, future, future + 1) ;

    return true ;
}


template<class MessageType, uint32_t RingSize>
uint32_t Ring<MessageType, RingSize>::popAsManyAsPossible(MessageType message[], uint32_t count)
{
    uint32_t current ;
    uint32_t future ;
    uint32_t storedCount ;

    for( ; ; )
    {
        future = consumer.future ;
        current = producer.current ;        

        storedCount = current - future ;
        if(unlikely(0 == storedCount))
        {
            return 0 ;
        }
        if(unlikely(count < storedCount))
        {
            storedCount = count ;
        }

        if(likely(RingUtil::CAS(&consumer.future, future, future + storedCount)))
        {
            break ;
        }
    }

    uint32_t readPos = RingUtil::GetPos<RING_MASK>(future) ;

    if(likely(RingSize > readPos + storedCount))
    {
        uint32_t pos ;
        for( pos = 0; pos < (storedCount & (~(uint32_t)0x03)); pos += 4, readPos += 4)
        {
            message[pos] = data[readPos] ;
            message[pos + 1] = data[readPos + 1] ;
            message[pos + 2] = data[readPos + 2] ;
            message[pos + 3] = data[readPos + 3] ;
        }
        switch(storedCount & (uint32_t)0x03)
        {
            case 3: { message[pos++] = data[readPos++] ; }                
            case 2: { message[pos++] = data[readPos++] ; }
            case 1: { message[pos++] = data[readPos++] ; }
        }
    }else
    {
        uint32_t pos ;
        for( pos = 0; readPos < RingSize; )
        {
            message[pos++] = data[readPos++] ;
        }
        for( readPos = 0; pos < storedCount; )
        {
            message[pos++] = data[readPos++] ;
        }        
    }

    UpdateCurrentCursor(consumer, future, future + storedCount) ;

    return storedCount ;
}


template<class MessageType, uint32_t RingSize>
bool Ring<MessageType, RingSize>::pop(MessageType &message)
{
    uint32_t current ;
    uint32_t future ;

    for( ; ; )
    {
        future = consumer.future ;
        current = producer.current ;

        if(unlikely(0 == current - future))
        {
            continue ;
        }

        if(likely(RingUtil::CAS(&consumer.future, future, future + 1)))
        {
            break ;
        }
    }
    
    uint32_t readPos = RingUtil::GetPos<RING_MASK>(future) ;
    message = data[readPos] ;

    UpdateCurrentCursor(consumer, future, future + 1) ;
        
    return true ;
}

}