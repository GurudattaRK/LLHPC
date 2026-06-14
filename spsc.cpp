#include <iostream>    // std::cout, std::endl  — for printing to terminal
#include <ostream>
#include <thread>      // std::thread            — for spawning threads
#include <atomic>      // std::atomic            — for lock-free flag variables
#include <cstdint>     // uint16_t, uint32_t     — fixed-width integer types
#include <random>      // mt19937, uniform_int_distribution — random number generation
#include <chrono>      // std::chrono::milliseconds — for sleep durations
using namespace std;

template<typename T,int ring_size>
class ring_queue
{
    
    public:
        
        T ring[ring_size];
        int head=0;
        int tail=0;

        
        bool read(T* element)
        {
            if(head==tail)
            {  
                return false;
            }
            int idx = tail & (ring_size-1);
            *element = ring[idx];
            tail++;
            std::cout << "reading idx: " << idx<< "\n";
            return true;
        }

        bool write(T* element)
        {
            if(head-tail == ring_size)
            {  
                return false;
            }
            int idx = head & (ring_size-1);
            ring[idx] = *element;
            head++;
            std::cout << "writing idx: " << idx<< "\n";
            return true;
        }
};

ring_queue<uint16_t, 8> spsc_queue;

atomic<bool> sync{false};

const int TOTAL_ITEMS = 20;
const int wait_time = 10; //millieseconds
const int number_of_slots = 8;

// ring_queue<uint16_t, number_of_slots> spsc_queue;


//TEST 
uint16_t input[TOTAL_ITEMS] ={0};
uint32_t output[TOTAL_ITEMS]={0};
int global_input_idx =0;
int global_output_idx =0;

void producer()
{
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
        while (!spsc_queue.write(val_ptr))
        {
            // Yield the CPU briefly so the consumer gets a chance to drain
            std::this_thread::sleep_for(std::chrono::microseconds(wait_time));
        }
        
        // input[global_input_idx] = value;
        // global_input_idx++;
        
        // Optional: uncomment to see what the producer is sending
        std::cout << "[producer]   enqueueed   at: " << spsc_queue.head<< "\n";
    }

    // Signal the consumer that no more items are coming.
    // store() with memory_order_release ensures all prior writes
    // (the last enqueue) are visible before this flag update.
    sync.store(true, std::memory_order_release); // store true in bool "sync"

    std::cout << "[producer] finished — enqueueed " << TOTAL_ITEMS << " items\n";
}


// ============================================================
//  CONSUMER THREAD FUNCTION
// ============================================================
void consumer()
{
    int received = 0;   // count how many items we have processed

    // Keep running until we've received every item the producer will send.
    // Condition: either the queue still has items, OR the producer isn't done yet.
    while (received < TOTAL_ITEMS)   
    {
        uint16_t value;   // dequeue() will fill this in if an item is available
        uint16_t* val_ptr = &value;

        if (spsc_queue.read(val_ptr))       
        {
            // Cast to uint32_t before squaring to avoid uint16_t overflow.
            // uint16_t max = 65535; 65535^2 = 4,294,836,225 which exceeds
            // uint16_t (max 65535) but fits in uint32_t (max ~4.29 billion).
            uint32_t square = static_cast<uint32_t>(value) * static_cast<uint32_t>(value);

            std::cout << "[consumer]   dequeued:   "<<spsc_queue.tail<< "\n";
            
            // output[global_output_idx] = square;
            // global_output_idx++;

            ++received;
        }
        else       
        {
            // Queue was empty — check whether the producer is truly done.
            // load() with memory_order_acquire pairs with the producer's
            // release store, guaranteeing we see the final enqueue before done=true.
            if (sync.load(std::memory_order_acquire))           
            {
                // Producer finished but queue might have had a tiny race window —
                // do one final check to see if any last elements were created by producer while consumer was checking.
                while (spsc_queue.read(&value)) {
                    uint32_t square = static_cast<uint32_t>(value) * static_cast<uint32_t>(value);
                    // output[global_output_idx] = square;
                    // global_output_idx++;
                    ++received;
                }
                break;   // definitely nothing left
            }
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
              << "Queue capacity: " << 16 << " slots\n\n";

    // Spawn the consumer FIRST so it is already waiting when the
    // producer starts enqueueing (avoids a tiny initial full-queue stall).
    std::thread consumer_thread{consumer};   // launches consumer()
    std::thread producer_thread{producer};   // launches producer()

    // join() blocks main until each thread finishes.
    // We MUST join (or detach) every thread before main returns,
    // otherwise std::terminate() is called — the program crashes.
    producer_thread.join();
    consumer_thread.join();

    std::cout << "\nAll done.\n";

    bool flag = true;
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
    return 0;
}