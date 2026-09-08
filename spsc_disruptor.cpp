#include <iostream>    // std::cout, std::endl  — for printing to terminal
#include <ostream>
#include <thread>      // std::thread            — for spawning threads
#include <atomic>      // std::atomic            — for lock-free flag variables
#include <cstdint>     // uint16_t, uint32_t     — fixed-width integer types
#include <random>      // mt19937, uniform_int_distribution — random number generation
#include <chrono>      // std::chrono::milliseconds — for sleep durations

#if defined(__x86_64__)
    #include <immintrin.h>
    #define SPIN_WAIT() _mm_pause()
#elif defined(__aarch64__)
    #define SPIN_WAIT() asm volatile("yield")
#else
    #define SPIN_WAIT() ((void)0)   // no-op fallback
#endif

using namespace std;

template<typename T>
struct slot
{
    T data=0;
    atomic<uint64_t> seq{0};
};

template<typename T, int ring_size>
class ring_queue
{
    slot<T> ring[ring_size];

    public:
        ring_queue()
        {
            cout<<"Getting slots in ring buffer ready"<<endl;
            for (int i = 0; i < ring_size; i++)
                ring[i].seq.store(i, memory_order_relaxed);  // slot i starts at sequence i
        }
    
        bool read(T* output, uint64_t counter)
        {
            int idx = counter & (ring_size-1);
            while(1)
            {
                if(ring[idx].seq.load(memory_order_acquire) == counter + 1)
                {
                    *output = ring[idx].data;
                    ring[idx].seq.store(counter + ring_size, memory_order_release);
                    return true;
                }
                else
                {
                    SPIN_WAIT();
                }
            }
        }

        bool write(T* value, uint64_t counter)
        {
            int idx = counter & (ring_size-1);
            while(1)
            {
                if(ring[idx].seq.load(memory_order_acquire) == counter)
                {
                    ring[idx].data = *value;
                    //atomic_thread_fence(memory_order_release); // uncomment to maybe enable MPMC support
                    ring[idx].seq.store(counter +1, memory_order_release);
                    return true;
                }
                else
                {
                    SPIN_WAIT();
                }
            }
        }
};


const int TOTAL_ITEMS = 20;
const int wait_time = 10; //millieseconds
const int number_of_slots = 8;

ring_queue<uint16_t, number_of_slots> spsc_queue;


//TEST ==============
uint16_t input[TOTAL_ITEMS] ={0};
uint32_t output[TOTAL_ITEMS]={0};
int global_input_idx =0;
int global_output_idx =0;
// ==================


void producer()
{
    uint64_t counter = 0;
    // ---- random number setup --------------------------------
    // mt19937 is the Mersenne Twister engine — fast, good distribution
    std::mt19937 rng{std::random_device{}()};

    // Distribute uniformly over the full range of uint16_t (0 … 65535)
    std::uniform_int_distribution<uint16_t> dist{0, 65535};
    // ---------------------------------------------------------

    for (int i = 0; i < TOTAL_ITEMS; ++i)
    {
        uint16_t value = dist(rng);   // generate one random 16-bit int
        uint16_t * val_ptr = &value;

        // Spin until the queue accepts the item.
        // enqueue() returns false when the ring buffer is full.
        spsc_queue.write(val_ptr,counter);
        
        // Yield the CPU briefly so the consumer gets a chance to drain
        std::this_thread::sleep_for(std::chrono::microseconds(wait_time));
        counter++;
        
        
        // input[global_input_idx] = value;
        // global_input_idx++;
        
        // Optional: uncomment to see what the producer is sending
        std::cout << "[producer]  counter  at: " <<counter << "\n";
    }

    std::cout << "[producer] finished — enqueueed " << TOTAL_ITEMS << " items\n";
}


// ============================================================
//  CONSUMER THREAD FUNCTION
// ============================================================
void consumer()
{
    int received = 0;   // count how many items we have processed

    uint64_t counter = 0;

    // Keep running until we've received every item the producer will send.
    while (received < TOTAL_ITEMS)   
    {
        uint16_t value;   // dequeue() will fill this in if an item is available
        uint16_t* val_ptr = &value;

        if (spsc_queue.read(val_ptr,counter))       
        {
            uint32_t square = static_cast<uint32_t>(value) * static_cast<uint32_t>(value);
            counter++;
            std::cout << "[consumer]   counter:   "<<counter<< "\n";
            
            // output[global_output_idx] = square;
            // global_output_idx++;
            ++received;
            // Queue is empty but producer is still running — back off
            std::this_thread::sleep_for(std::chrono::microseconds(wait_time));
        }
    }

    std::cout << "[consumer] finished — processed " << received << " items\n";
}

int main()
{
    std::cout << "SPSC Queue test — "
              << TOTAL_ITEMS << " uint16_t values\n"
              << "Queue capacity: " << 16 << " slot\n\n";

    // Spawn the consumer FIRST so it is already waiting when the
    // producer starts enqueueing (avoids a tiny initial full-queue stall).
    std::thread consumer_thread{consumer};   // launches consumer()
    std::thread producer_thread{producer};   // launches producer()

    producer_thread.join();
    consumer_thread.join();

    std::cout << "\nAll done.\n";
//TEST ============================
/*     bool flag = true;
    for (int i = 0; i < TOTAL_ITEMS; i++) 
    {
        uint32_t expected = static_cast<uint32_t>(input[i]) * static_cast<uint32_t>(input[i]);
        if (output[i] != expected) 
        {
            flag = false;
            break;
        }
    }
    if(flag)
        cout<<"test passed, all values are correct"<< endl;
*/
//=====================================
    return 0;
}