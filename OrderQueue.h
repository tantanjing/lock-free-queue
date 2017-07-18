#pragma once

#include "OrderRing.h"

namespace queue
{

template<class MessageType, uint32_t QueueSize>
class OrderQueue
{
 public:

    OrderQueue(void) {}
    
    OrderQueue(const std::string name): queue(name) {}

    inline uint32_t pushAsManyAsPossible(MessageType message[], uint32_t count)
    {
        return queue.pushAsManyAsPossible(message, count) ;
    }

    inline uint32_t push(MessageType message[], uint32_t count)
    {
        return queue.push(message, count) ;
    }

    inline bool push(MessageType &message)
    {
        return queue.push(message) ;
    }

    uint32_t push(MessageType &message, MessageType popMessage[], uint32_t count)
    {
        return queue.push(message, popMessage, count) ;
    }
    
    inline uint32_t popAsManyAsPossible(MessageType message[], uint32_t count)
    {
        return queue.popAsManyAsPossible(message, count) ;
    }

    inline uint32_t pop(MessageType message[], uint32_t count)
    {
        return queue.pop(message, count) ;
    }

    inline bool pop(MessageType &message)
    {
        return queue.pop(message) ;
    }

 private:

    OrderRing<MessageType, QueueSize> queue ;
 
} ;

}