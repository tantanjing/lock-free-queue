#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>

#include <stdio.h>
#include "Queue.h"


template<class MessageQueue>
class Consumer
{
 public:
    Consumer(uint32_t id, uint64_t cycleCount, MessageQueue &queue): 
        id(id), cycleCount(cycleCount), count(0), pthread(0), running(false), queue(queue) {}
    
    static void *run(Consumer<MessageQueue> *consumer)
    {        
        consumer->work3() ;
        return NULL ;
    }

    void work(void)
    {
        printf("Consumer %u start working\n", id) ;
        
        running = true ;
        while(running)
        {
            count ++ ;
            uint64_t message = 0 ;
            //printf("Consumer %u consumer %lu before\n", id, message) ;
            queue.pop(message) ;
            //printf("Consumer %u consumer %lu after\n", id, message) ;
            if(cycleCount && count >= cycleCount)
            {
                break ;
            }
        }
        printf("Consumer %u stop working, count=%lu\n", id, count) ;
    }

    void work2(void)
    {
        printf("Consumer %u start working\n", id) ;
        
        running = true ;
        while(running)
        {
            uint64_t message[10240] ;
            //printf("Consumer %u consumer %lu before\n", id, message) ;
            count += queue.popAsManyAsPossible(message, sizeof(message)/sizeof(message[0])) ;
            //printf("Consumer %u consumer %lu after\n", id, message) ;
            if(cycleCount && count >= cycleCount)
            {
                break ;
            }
        }
        printf("Consumer %u stop working, count=%lu\n", id, count) ;
    }

    void work3(void)
    {
        printf("Consumer %u start working\n", id) ;
        
        running = true ;
        while(running)
        {
            uint64_t message[4] ;
            //printf("Consumer %u consumer %lu before\n", id, message) ;
            count += queue.pop(message, sizeof(message)/sizeof(message[0])) ;            
            //printf("Consumer %u consumer %lu after\n", id, message) ;
            if(cycleCount && count >= cycleCount)
            {
                break ;
            }
        }
        printf("Consumer %u stop working, count=%lu\n", id, count) ;
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
} ;

template<class MessageQueue>
class Producer
{
 public:
    Producer(uint32_t id, uint64_t cycleCount, MessageQueue &queue): 
        id(id), cycleCount(cycleCount), count(0), pthread(0), running(false), queue(queue) {}
    
    static void *run(Producer<MessageQueue> *producer)
    {        
        producer->work() ;
        return NULL ;
    }

    void work(void)
    {
        printf("Producer %u start working\n", id) ;
        
        running = true ;
        while(running)
        {
            count ++ ;
            //printf("Producer %u product %lu before\n", id, count) ;
            queue.push(count) ;
            //printf("Producer %u product %lu after\n", id, count) ;
            if(cycleCount && count >= cycleCount)
            {
                break ;
            }
        }
        printf("Producer %u stop working, count=%lu\n", id, count) ;
    }


    void work2(void)
    {
        printf("Producer %u start working\n", id) ;
        
        running = true ;
        while(running)
        {            
            uint64_t message[5] ;
            for(uint32_t pos = 0; pos < sizeof(message)/sizeof(message[0]); ++pos)
            {
                message[pos] = count ++ ;
            }
            //printf("Producer %u product %lu before\n", id, count) ;
            queue.push(message, sizeof(message)/sizeof(message[0])) ;
            //printf("Producer %u product %lu after\n", id, count) ;
            if(cycleCount && count >= cycleCount)
            {
                break ;
            }
        }
        printf("Producer %u stop working, count=%lu\n", id, count) ;
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
} ;
//16384
#define MessageQueueType queue::Queue<uint64_t, 4096>

//MessageQueueType queue CACHE_ALIGNED;

int main(int argv, char *args[])
{
    printf("Test queue\n") ;
    
    
    
    MessageQueueType queue ;
    uint32_t wokerId = 0 ;
    Producer<MessageQueueType > producer(wokerId++, (uint64_t)10000000, queue) ;
    producer.start() ;
    Producer<MessageQueueType > producer2(wokerId++, (uint64_t)10000000, queue) ;
    producer2.start() ;
    Producer<MessageQueueType > producer3(wokerId++, (uint64_t)10000000, queue) ;
    producer3.start() ;
    Producer<MessageQueueType > producer4(wokerId++, (uint64_t)10000000, queue) ;
    producer4.start() ;

    Consumer<MessageQueueType > consumer(wokerId++, 2*(uint64_t)10000000, queue) ;
    consumer.start() ;

    Consumer<MessageQueueType > consumer2(wokerId++, 2*(uint64_t)10000000, queue) ;
    consumer2.start() ;    /*
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

    consumer2.wait() ;    /*
    consumer3.wait() ;
    consumer4.wait() ;
    */
    //producer.start() ;
    return 0 ;
}