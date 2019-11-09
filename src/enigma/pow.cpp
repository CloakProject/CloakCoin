/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// CONTRIBUTORS AND COPYRIGHT HOLDERS (c) 2013:
// Bob Mottram (bob@robotics.uk.to)
// Dag Robøle (BM-2DAS9BAs92wLKajVy9DS1LFcDiey5dxp5c)

#include <cstring>
#include <algorithm>
#include <iterator>
#include <limits>
//#include <thread>
//#include <atomic>
#include "pow.h"
#include "hashblock.h"
#include "util.h"
/*
namespace internal {

std::atomic_bool tflag;

SecureVector HashX13(const SecureVector& data)
{
    return vchFromString(Hash9(data.begin(), data.end()).ToString());
}

SecureVector HashX13_2(const SecureVector& data)
{
    return HashX13(HashX13(data));
}

uint64_t do_generate_nonce(const SecureVector& payload_hash, uint64_t target)
{
    uint64_t nonce_test = 0;
    uint64_t trials_test = std::numeric_limits<uint64_t>::max();
    SecureVector vx;
    SecureVector v(sizeof(uint64_t) + payload_hash.size());
    memcpy(v.data() + sizeof(uint64_t), payload_hash.data(), payload_hash.size());
    uint64_t *nonce = (uint64_t*)v.data();

    while(trials_test > target)
    {
        ++nonce_test;
        // *nonce = host_to_big_64(nonce_test);
        *nonce = nonce_test;
        vx = HashX13_2(v);
        memcpy(&trials_test, vx.data(), 8);
        //trials_test = big_to_host_64(trials_test);
    }

    return nonce_test;
}

void do_generate_nonce_parallel_worker(const SecureVector& payload_hash, uint64_t target, uint64_t offset, uint64_t iterations, uint64_t* nonce)
{
    *nonce = 0;

    uint64_t i = offset, nonce_test = offset;
    uint64_t trials_test = std::numeric_limits<uint64_t>::max();
    SecureVector vx, v(sizeof(uint64_t) + payload_hash.size());
    memcpy(v.data() + sizeof(uint64_t), payload_hash.data(), payload_hash.size());
    uint64_t *nonce_ptr = (uint64_t*)v.data();

    while(trials_test > target)
    {
        if(tflag || ++i - offset > iterations)
            return;
        ++nonce_test;
        // *nonce_ptr = host_to_big_64(nonce_test);
        vx = HashX13_2(v);
        memcpy(&trials_test, vx.data(), 8);
        //trials_test = big_to_host_64(trials_test);
    }

    tflag = true;
    *nonce = nonce_test;
}

uint64_t do_generate_nonce_parallel(const SecureVector& payload_hash, uint64_t target) // FIXME: Do better threading
{
    unsigned int numthreads = std::thread::hardware_concurrency() - 1;
    if(numthreads <= 0)
        numthreads = 1;

    std::vector<std::thread> threads;
    uint64_t vnonce[numthreads];
    uint64_t iterations = std::numeric_limits<uint64_t>::max() / numthreads;

    tflag = false;

    for(unsigned int i = 0; i < numthreads; i++)
        threads.push_back(std::thread(do_generate_nonce_parallel_worker, payload_hash, target, i * iterations, iterations, &vnonce[i]));

    for(auto& thread : threads)
        thread.join();

    for(unsigned int i = 0; i < numthreads; i++)
        if(vnonce[i])
            return vnonce[i];
    return 0;
}

} // namespace internal

uint64_t generate_nonce(const SecureVector& payload, bool parallel)
{
    SecureVector payload_hash = internal::HashX13(payload);
    uint64_t target = std::numeric_limits<uint64_t>::max() / ((payload.size() + PAYLOAD_LENGTH_EXTRA_BYTES + 8) * AVERAGE_PROOF_OF_WORK_NONCE_TRIALS_PER_BYTE);

    if(parallel)
        return internal::do_generate_nonce_parallel(payload_hash, target);
    return internal::do_generate_nonce(payload_hash, target);
}

bool validate_nonce(const SecureVector& payload)
{
    if (payload.size() < sizeof(uint64_t))
        return false;

    SecureVector initial_payload;
    uint64_t trials_test, nonce = *((uint64_t*)payload.data());
    initial_payload.assign(payload.begin() + sizeof(uint64_t), payload.end());
    SecureVector initial_hash = internal::HashX13(initial_payload);
    uint64_t target = std::numeric_limits<uint64_t>::max() / ((initial_payload.size() + PAYLOAD_LENGTH_EXTRA_BYTES + 8) * AVERAGE_PROOF_OF_WORK_NONCE_TRIALS_PER_BYTE);

    SecureVector v(sizeof(uint64_t) + initial_hash.size());
    memcpy(v.data() + sizeof(uint64_t), initial_hash.data(), initial_hash.size());
    *((uint64_t*)v.data()) = nonce;

    SecureVector vx = internal::HashX13_2(v);
    //trials_test = big_to_host_64(*vx.data());

    return trials_test <= target;
}
*/
