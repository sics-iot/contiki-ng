#include <string.h>
#include "lib/assert.h"
#include "sys/memory-barrier.h"
#include "sys/critical.h"

#include "mpmc-ring.h"

/*----------------------------------------------------------------------------------------*/

/*
 * Implementation note: The implementation of this bounded mpmc queue
 * is borrowed from the `heapless::mpmc` from Rust's `heapless` crate,
 * https://docs.rs/heapless/. The Rust implementation is also based on
 * a C++ implementation by Dmitry Vyukov,
 * http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue.
 *
 * The mpmc queue maintains "sequence number" for each cell in the
 * queue. The sequence number is maintained in such a way as:
 *
 * - cell[i] is empty    <=> seq[i] % size == i
 * - cell[i] is occupied <=> seq[i] % size == i + 1
 *
 * The queue also maintains `get_pos` and `put_pos`. They runs in the
 * whole domain of `uint8_t`. `(get_pos % size)` is the index of the
 * cell to get an element from. `(put_pos % size)` is the index of the
 * cell to put an element to. Those two position variables follow the
 * rules below.
 *
 * - cell[get_pos % size] is empty    ==> seq[get_pos % size] == get_pos
 * - cell[get_pos % size] is occupied ==> seq[get_pos % size] == get_pos + 1
 * - cell[put_pos % size] is empty    ==> seq[put_pos % suze] == put_pos
 * - cell[put_pos % size] is occupied ==> seq[put_pos % suze] == put_pos + 1 - size
 *
 * Example (1): The queue size == 8. "g(2)" means `get_pos == 2`,
 * "p(6)" means `put_pos == 6`. Cells [2,3,4,5] are occupied, other
 * cells are empty.
 *
 *     index    0   1   2   3   4   5   6   7
 *           +---+---+---+---+---+---+---+---+
 *     seq.  |  8|  9|  3|  4|  5|  6|  6|  7|
 *           +---+---+---+---+---+---+---+---+
 *                    ^               ^
 *                    g(2)            p(6)
 *
 * Example (2): Cells [5,6,7,0,1] are occupied (wrapped at the
 * boundary of 8), other cells are empty. The sequence number is used
 * to tell if the cell is in a wrapped region or not. That is why the
 * queue size must be <= 128.
 *
 *     index    0   1   2   3   4   5   6   7
 *           +---+---+---+---+---+---+---+---+
 *     seq.  | 41| 42| 42| 43| 44| 38| 39| 40|
 *           +---+---+---+---+---+---+---+---+
 *                    ^           ^
 *                    p(42)       g(37)
 *
 * Example (3): All cells are occupied. The sequence number is wrapped
 * at 256 boundary. Difference of sequence numbers is at most the size
 * of the queue. So, if the queue size < 128, we can easily detect the
 * wrapping at 256 boundary.
 *
 *     index    0   1   2   3   4   5   6   7
 *           +---+---+---+---+---+---+---+---+
 *     seq.  |  1|  2|  3|252|253|254|255|  0|
 *           +---+---+---+---+---+---+---+---+
 *                        ^
 *                        g(251)
 *                        p(3)
 * 
 *
 * CAVEAT: In very limited scenarios, internal of mpmc-ring can get
 * broken. For example,
 *
 * 1. Start `put_begin` on a non-full queue, such as the following.
 *
 *     index    0   1   2   3   4   5   6   7
 *           +---+---+---+---+---+---+---+---+
 *     seq.  | 48| 49| 43| 44| 45| 45| 46| 47|
 *           +---+---+---+---+---+---+---+---+
 *                      ^           ^
 *                      g(42)       p(45)
 *
 * 2. An interrupt occurs during processing the `put_begin`, just
 * after it calculates the `dif` (which is 0) but before executing
 * `atomic_cas_uint8`.
 *
 * 3. In the interrupt, it executes (256 * N) put operations and
 * some get operations (N is a natural number). In the end, the
 * queue becomes full with the same `put_pos` as it had before the
 * interrupt.
 *
 *     index    0   1   2   3   4   5   6   7
 *           +---+---+---+---+---+---+---+---+
 *     seq.  | 41| 42| 43| 44| 45| 38| 39| 40|
 *           +---+---+---+---+---+---+---+---+
 *                                  ^
 *                                  g(37)
 *                                  p(45)
 *
 * 4. The original `put_begin` resumes. Because `dif == 0`, it runs
 * `atomic_cas_uint8`. Because the `put_pos` remains the same, it
 * increments `put_pos`. However, because the queue is actually
 * full, this is incorrect. This leads to loss of data stored in
 * cell[5] in the above example.
 *
 *
 * The above problem occurs when (1) an interrupt occurs inside
 * `put_begin`, before executing `atomic_cas_uint8`, and (2) the
 * interrupt changes the queue state from non-full to full, and (3)
 * the interrupt runs (256 * N) put operations so that `put_pos`
 * remains the same. The dual is also true for `get_begin` and
 * non-empty queue. Anyway, I think the above condition is so rare.
 *
 */

void
mpmc_ring_init(mpmc_ring_t *ring)
{
  uint8_t i;

  assert(ring->mask < 64);
  assert(ring->mask > 0);
  assert((ring->mask & (ring->mask + 1)) == 0);
  
  ring->put_pos = 0;
  ring->get_pos = 0;
  for(i = 0 ; i <= ring->mask ; i++) {
    ring->sequences[i] = i;
  }
}

int
mpmc_ring_put_begin(mpmc_ring_t *ring, mpmc_ring_index_t *got_index)
{
  uint8_t pos;
  uint8_t index;

  assert(ring != NULL);
  assert(got_index != NULL);
  
  pos = ring->put_pos;
  memory_barrier();
  while(1) {
    int8_t dif;
    index = pos & ring->mask;
    dif = (int8_t)(ring->sequences[index]) - (int8_t)pos;
    memory_barrier();
    if(dif == 0) {
      if(atomic_cas_uint8(&ring->put_pos, pos, pos + 1)) {
        break;
      }
    } else if(dif < 0) {
      return 0;
    } else {
      pos = ring->put_pos;
      memory_barrier();
    }
  }
  got_index->i = index;
  got_index->_pos = pos;
  return 1;
}

void
mpmc_ring_put_commit(mpmc_ring_t *ring, const mpmc_ring_index_t *index)
{
  assert(ring != NULL);
  assert(index != NULL);

  ring->sequences[index->i] = index->_pos + 1;
}

int
mpmc_ring_get_begin(mpmc_ring_t *ring, mpmc_ring_index_t *got_index)
{
  /* Basically the dual of put_begin */
  uint8_t pos;
  uint8_t index;

  assert(ring != NULL);
  assert(got_index != NULL);
  
  pos = ring->get_pos;
  memory_barrier();
  while(1) {
    int8_t dif;
    index = pos & ring->mask;
    dif = (int8_t)(ring->sequences[index]) - (int8_t)(pos + 1);
    memory_barrier();
    if(dif == 0) {
      if(atomic_cas_uint8(&ring->get_pos, pos, pos + 1)) {
        break;
      }
    } else if(dif < 0) {
      return 0;
    } else {
      pos = ring->get_pos;
      memory_barrier();
    }
  }
  got_index->i = index;
  got_index->_pos = pos;
  return 1;
}

void
mpmc_ring_get_commit(mpmc_ring_t *ring, const mpmc_ring_index_t *index)
{
  assert(ring != NULL);
  assert(index != NULL);

  ring->sequences[index->i] = index->_pos + ring->mask + 1;
}

int
mpmc_ring_elements(const mpmc_ring_t *ring)
{
  assert(ring != NULL);
  int8_t dif = ((int8_t)ring->put_pos - (int8_t)ring->get_pos);
  return dif;

  /*
   * The code below is necessary if we allow mpmc_ring of size
   * 128. The else clause is especially for the case where size == 128
   * && elements = 128.
   *
   * if(dif >= 0) {
   *   return (int)dif;
   * } else {
   *   int ret = (int)dif;
   *   while(ret <= 0) {
   *     ret += ring->mask + 1;
   *   }
   *   return ret;
   * }
   */
}

int
mpmc_ring_empty(const mpmc_ring_t *ring)
{
  return mpmc_ring_elements(ring) == 0;
}

uint8_t
mpmc_ring_size(const mpmc_ring_t *ring)
{
  assert(ring != NULL);
  return ring->mask + 1;
}

