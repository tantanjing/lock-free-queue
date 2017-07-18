#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>

#include <stdio.h>

#include <new>

#include "OrderQueue.h"

class SequenceUtil
{
 public:
    inline static uint32_t get(void)
    {
        return __sync_fetch_and_add(&sequence, 1) ;
    }

 private:
    static uint32_t sequence ;
} ;

uint32_t SequenceUtil::sequence = 0 ;

class Message
{
 public:

    Message(void)
    {
        data = NULL ;
        sequence = 0 ;
        flag = 0 ;
    }
 
    Message(const void* data, uint32_t sequence): data(const_cast<void*>(data)), sequence(sequence), flag(0) {}

    void defaultInit(void)
    {
        sequence = SequenceUtil::get() ;
    }
    
    inline const void* getData(void)
    {
        return data ;
    }
    inline const void* getData(void) const
    {
        return data ;
    }

    inline uint32_t getSequence(void)
    {
        return sequence ;
    }
    inline uint32_t getSequence(void) const
    {
        return sequence ;
    }

 private:

    void* data ;
    uint32_t sequence ;
    uint32_t flag ;
} ;

#define GetArrayCount(array) (sizeof(message)/sizeof(message[0]))

#define InitMessages(message) \
    do{\
        for(uint32_t pos = 0; pos < GetArrayCount(message); ++pos)\
        {\
            message[pos].defaultInit() ;\
        }\
    } while(0)

template<class MessageQueue, uint32_t MessageCount>
class Consumer
{
 public:
    Consumer(uint32_t id, uint64_t cycleCount, MessageQueue &queue, uint32_t runType): 
        id(id), cycleCount(cycleCount), count(0), pthread(0), running(false), queue(queue), runType(runType) 
    {
        conMessages = new Message[MessageCount] ;
        conMessageCount = 0 ;
    }
    
    static void *run(Consumer<MessageQueue, MessageCount> *consumer)
    {
        consumer->work() ;
        return NULL ;
    }

    void work(void)
    {
        printf("Consumer %u start working, type=%u, max-count=%lu\n", id, runType, cycleCount) ;
        switch(runType)
        {
            case 1:
            {
                work1() ;
                break ;
            }
            case 2:
            {
                work2() ;
                break ;
            }
            case 3:
            {
                work3() ;
                break ;
            }
        }        
        printf("Consumer %u stop working, count=%lu\n", id, count) ;
    }
    
    bool checkMessage(void)
    {
        for(uint32_t pos = 0; pos < conMessageCount; ++pos)
        {
            if(conMessages[pos].getSequence() != pos)
            {
                printf("Check message failed\n") ;
                return false;
            }
        }
        printf("Check message success\n") ;
        return true ;
    }
    
    void work1(void)
    {
        running = true ;
        while(running)
        {
            count ++ ;
            Message message ;
            //printf("Consumer %u consumer before\n", id) ;
            queue.pop(message) ;
            //printf("Consumer %u consumer %u after\n", id, message.getSequence()) ;
            conMessages[conMessageCount++] = message ;
            
            //printf("Consumer %u consumer sequence=%u, count=%u\n", id, message.getSequence(), conMessageCount) ;
            
            if(cycleCount && count >= cycleCount)
            {
                break ;
            }
        }
        
        checkMessage() ;
    }
    
    void work2(void)
    {
        running = true ;
        while(running)
        {
            Message message[10000] ;
            //printf("Consumer %u consumer %lu before\n", id, message) ;
            uint32_t popCount = queue.popAsManyAsPossible(message, GetArrayCount(message)) ;

            for(uint32_t pos = 0; pos < popCount; ++pos)
            {
                conMessages[conMessageCount++] = message[pos] ;
            }

            count += popCount ;
            //printf("Consumer %u consumer %lu after\n", id, message) ;
            
            if(cycleCount && count >= cycleCount)
            {
                break ;
            }
        }
        checkMessage() ;
    }

    void work3(void)
    {        
        running = true ;
        while(running)
        {
            Message message[4] ;
            //printf("Consumer %u consumer %lu before\n", id, message) ;
            count += queue.pop(message, GetArrayCount(message)) ;            
            //printf("Consumer %u consumer %lu after\n", id, message) ;
            if(cycleCount && count >= cycleCount)
            {
                break ;
            }
        }
        checkMessage() ;
    }    
    
    void start()
    {
        if(0 != pthread_create(&pthread, NULL, (void* (*)(void*))run, (void*)this))
        {
            printf("Start Consumer thread failed , id=%u\n", id ) ;
            return ;
        }
    }
    
    void stop(void)
    {
        printf("Consumer %u stoped, count=%lu\n", id, count) ;
        
        if(running)
        {
            running = false ;
            void *retval ;
            pthread_join(pthread, &retval) ;
        }        
    }
    
    void wait(void)
    {
        void *retval ;
        pthread_join(pthread, &retval) ;
    }
 
 private:
    uint32_t id ;
    uint64_t cycleCount ;
    uint64_t count ;
    pthread_t pthread ;
    volatile bool running ;

    MessageQueue &queue ;
    
    uint32_t runType ;
    
    Message *conMessages ;
    uint32_t conMessageCount ;
} ;

template<class MessageQueue>
class Producer
{
 public:
    Producer(uint32_t id, uint64_t cycleCount, MessageQueue &queue, uint32_t runType): 
        id(id), cycleCount(cycleCount), count(0), pthread(0), running(false), queue(queue), runType(runType) {}
    
    static void *run(Producer<MessageQueue> *producer)
    {        
        producer->work() ;
        return NULL ;
    }

    void work(void)
    {
        printf("Producer %u start working, type=%u\n", id, runType) ;
        switch(runType)
        {
            case 1:
            {
                work1() ;
                break ;
            }
            case 2:
            {
                work2() ;
                break ;
            }
        }        
        printf("Producer %u stop working, count=%lu\n", id, count) ;
    }
    
    void work1(void)
    {        
        running = true ;
        while(running)
        {
            count ++ ;
            Message message ;
            message.defaultInit() ;
            //printf("Producer %u product %lu before\n", id, count) ;
            queue.push(message) ;
            //printf("Producer %u product %u after\n", id, message.getSequence()) ;
            if(cycleCount && count >= cycleCount)
            {
                break ;
            }
        }
    }


    void work2(void)
    {        
        running = true ;
        while(running)
        {            
            Message message[5] ;
            
            InitMessages(message) ;
            //printf("Producer %u product %lu before\n", id, count) ;
            queue.push(message, GetArrayCount(message)) ;
            //printf("Producer %u product %lu after\n", id, count) ;
            count += GetArrayCount(message) ;
            if(cycleCount && count >= cycleCount)
            {
                break ;
            }
        }
    }
    
    void start()
    {
        if(0 != pthread_create(&pthread, NULL, (void* (*)(void*))run, (void*)this))
        {
            printf("Start producer thread failed , id=%u\n", id ) ;
            return ;
        }
    }
    
    void stop(void)
    {
        printf("Producer %u stoped, count=%lu\n", id, count) ;
        
        if(running)
        {
            running = false ;
            void *retval ;
            pthread_join(pthread, &retval) ;
        }        
    }
    
    void wait(void)
    {
        void *retval ;
        pthread_join(pthread, &retval) ;
    }
 
 private:
    uint32_t id ;
    uint64_t cycleCount ;
    uint64_t count ;
    pthread_t pthread ;
    volatile bool running ;

    MessageQueue &queue ;
    
    uint32_t runType ;
} ;

//16384
#define MessageQueueType queue::OrderQueue<Message, 4096>

int main(int argv, char *args[])
{
    printf("Test order queue\n") ;    
    
    const uint64_t messageCount = 20000000 ;
    
    MessageQueueType queue ;
    uint32_t wokerId = 0 ;
    Producer<MessageQueueType > producer(wokerId++, messageCount, queue, 1) ;
    producer.start() ;
    Producer<MessageQueueType > producer2(wokerId++, messageCount, queue, 1) ;
    producer2.start() ;
    Producer<MessageQueueType > producer3(wokerId++, messageCount, queue, 2) ;
    producer3.start() ;
    Producer<MessageQueueType > producer4(wokerId++, messageCount, queue, 1) ;
    producer4.start() ;

    Consumer<MessageQueueType, 4*messageCount > consumer(wokerId++, 4*messageCount, queue, 2) ;
    consumer.start() ;
/*
    Consumer<MessageQueueType > consumer2(wokerId++, 2*(uint64_t)100000000, queue, 1) ;
    consumer2.start() ;    
    Consumer<MessageQueueType > consumer3(wokerId++, (uint64_t)100000000, queue) ;
    consumer3.start() ;
    Consumer<MessageQueueType > consumer4(wokerId++, (uint64_t)100000000, queue) ;
    consumer4.start() ;
    */
    
    producer.wait() ;
    producer2.wait() ;
    producer3.wait() ;
    producer4.wait() ;
    consumer.wait() ;
  
    /*
    consumer2.wait() ;  
    consumer3.wait() ;
    consumer4.wait() ;
    */
    //producer.start() ;
    return 0 ;
}