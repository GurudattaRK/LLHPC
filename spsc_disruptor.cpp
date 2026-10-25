#include <iostream>    // std::cout, std::endl  — for printing to terminal
#include <ostream>
#include <thread>      // std::thread            — for spawning threads
#include <atomic>      // std::atomic            — for lock-free flag variables
#include <cstdint>     // uint16_t, uint32_t     — fixed-width integer types
#include <random>      // mt19937, uniform_int_distribution — random number generation
#include <chrono>      // std::chrono::milliseconds — for sleep durations
#include <cstddef>     // std::byte

#if defined(__x86_64__)
    #include <immintrin.h>
    #define SPIN_WAIT() _mm_pause()
#elif defined(__aarch64__)
    #define SPIN_WAIT() asm volatile("yield")
#else
    #define SPIN_WAIT() ((void)0)   // no-op fallback
#endif

using namespace std;

// Padded to exactly one cache line so adjacent slots never false-share:
// without this, slot<uint16_t> is 16 bytes and four slots pack into one
// 64-byte line, so the producer writing slot[i] and the consumer reading
// slot[i+1..i+3] bounce the same cache line back and forth between cores.
// `seq` (8-byte aligned) is placed first so no implicit padding sneaks in
// before it - otherwise alignas(64) would round the whole struct up to 128.
template<typename T>
struct alignas(64) slot
{
    static_assert(sizeof(T) <= 64 - sizeof(atomic<uint64_t>),
                  "T too large to fit a one-cache-line slot");
    atomic<uint64_t> seq{0};
    T data{};
    char _pad[64 - sizeof(atomic<uint64_t>) - sizeof(T)];
};

template<typename T, int ring_size>
class ring_queue
{
    static_assert(ring_size > 0 && (ring_size & (ring_size - 1)) == 0,
                  "ring_size must be a power of two (index is counter & (ring_size-1))");

    slot<T> ring[ring_size];

    public:
        ring_queue()
        {
            cout<<"Getting slots in ring buffer ready"<<endl;
            for (int i = 0; i < ring_size; i++)
                ring[i].seq.store(i, memory_order_relaxed);  // slot i starts at sequence i
        }

        // Blocking: spins until the slot is readable. Single-producer/single-
        // consumer only, so this can never fail - hence void, not bool.
        void read(T* output, uint64_t counter)
        {
            while(!try_read(output, counter))
                SPIN_WAIT();
        }

        void write(T* value, uint64_t counter)
        {
            while(!try_write(value, counter))
                SPIN_WAIT();
        }

        // Non-blocking single-shot check. Returns false immediately if the
        // slot isn't ready yet instead of spinning - use this when the caller
        // wants to do other work rather than block.
        bool try_read(T* output, uint64_t counter)
        {
            int idx = counter & (ring_size-1);
            if (ring[idx].seq.load(memory_order_acquire) != counter + 1)
                return false;
            *output = ring[idx].data;
            ring[idx].seq.store(counter + ring_size, memory_order_release);
            return true;
        }

        bool try_write(T* value, uint64_t counter)
        {
            int idx = counter & (ring_size-1);
            if (ring[idx].seq.load(memory_order_acquire) != counter)
                return false;
            ring[idx].data = *value;
            // NOTE: this is SPSC only. A single producer owns `counter`
            // (nothing else increments it), so no CAS is needed here. Making
            // this MPMC is NOT just "add a fence" - multiple producers would
            // race on the same idx/counter, so you'd need an atomic
            // fetch_add on a shared write cursor plus a CAS on seq, same
            // shape as the classic Vyukov MPMC queue.
            ring[idx].seq.store(counter + 1, memory_order_release);
            return true;
        }
};


const int TOTAL_ITEMS = 20;
const int wait_time = 10; //millieseconds
const int number_of_slots = 8;

ring_queue<uint16_t, number_of_slots> spsc_queue;


// Values the producer sent / the consumer computed, indexed by item number
// (not by ring slot) so the correctness check below is independent of
// whatever wraparound happened inside the ring.
uint16_t input[TOTAL_ITEMS]  = {0};
uint32_t output[TOTAL_ITEMS] = {0};

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
        input[i] = value;             // record for the post-run correctness check
        uint16_t * val_ptr = &value;

        // Blocks until the ring accepts the item (SPSC - can't fail).
        spsc_queue.write(val_ptr, counter);

        // Simulated producer-side work between items. No I/O in this loop -
        // logging happens after both threads finish, not per iteration.
        std::this_thread::sleep_for(std::chrono::microseconds(wait_time));
        counter++;
    }

    std::cout << "[producer] finished — enqueued " << TOTAL_ITEMS << " items\n";
}


// ============================================================
//  CONSUMER THREAD FUNCTION
// ============================================================
void consumer()
{
    uint64_t counter = 0;

    for (int received = 0; received < TOTAL_ITEMS; ++received)
    {
        uint16_t value;
        spsc_queue.read(&value, counter);   // blocks until an item is ready (SPSC - can't fail)
        counter++;

        output[received] = static_cast<uint32_t>(value) * static_cast<uint32_t>(value);

        // Simulated consumer-side work between items, no I/O in the loop.
        std::this_thread::sleep_for(std::chrono::microseconds(wait_time));
    }

    std::cout << "[consumer] finished — processed " << TOTAL_ITEMS << " items\n";
}

int main()
{
    std::cout << "SPSC Queue test — "
              << TOTAL_ITEMS << " uint16_t values\n"
              << "Queue capacity: " << number_of_slots << " slots\n\n";

    // Spawn the consumer FIRST so it is already waiting when the
    // producer starts enqueueing (avoids a tiny initial full-queue stall).
    std::thread consumer_thread{consumer};   // launches consumer()
    std::thread producer_thread{producer};   // launches producer()

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
            std::cout << "[FAIL] item " << i << ": expected " << expected
                      << ", got " << output[i] << "\n";
        }
    }
    if (flag)
        cout << "test passed, all values are correct" << endl;

    return flag ? 0 : 1;
}