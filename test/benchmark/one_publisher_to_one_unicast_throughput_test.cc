// Copyright (c) 2011-2015, François Saint-Jacques
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the disruptor-- nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL FRANÇOIS SAINT-JACQUES BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <sys/time.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include <disruptor/sequence.h>
#include <disruptor/sequence_barrier.h>
#include <disruptor/sequencer.h>

using namespace disruptor;

namespace {

// Payload carried through the ring. What is measured here is the cost of the
// sequencing machinery around the event, not the event itself.
struct StubEvent {
  int64_t value;
};

constexpr size_t kBufferSize = 1024 * 8;
// 300M events, override with the first command line argument.
constexpr int64_t kDefaultIterations = 1000L * 1000L * 300L;

using BenchmarkSequencer =
    Sequencer<StubEvent, kBufferSize, SingleThreadedStrategy<kBufferSize>,
              kDefaultWaitStrategy>;

double Now() {
  struct timeval time;
  gettimeofday(&time, NULL);
  return time.tv_sec + (static_cast<double>(time.tv_usec) / 1000000);
}

}  // namespace

int main(int argc, char** argv) {
  const int64_t iterations = argc > 1 ? std::atol(argv[1]) : kDefaultIterations;
  // The last sequence that will ever be published, sequences start at 0.
  const int64_t last_sequence = iterations - 1;

  std::array<StubEvent, kBufferSize> events{};
  BenchmarkSequencer sequencer(events);

  // The publisher has to be gated on the consumer, otherwise it would claim
  // and overwrite ring entries that have not been consumed yet.
  Sequence consumer_sequence;
  sequencer.set_gating_sequences({&consumer_sequence});

  // A single consumer only depends on the cursor.
  SequenceBarrier<kDefaultWaitStrategy> barrier = sequencer.NewBarrier({});

  int64_t checksum = 0;
  std::thread consumer([&]() {
    int64_t next = kFirstSequenceValue;
    while (next <= last_sequence) {
      const int64_t available = barrier.WaitFor(next);
      if (available < next) continue;  // alerted, cannot happen here

      for (int64_t sequence = next; sequence <= available; ++sequence) {
        checksum += sequencer[sequence].value;
      }

      consumer_sequence.set_sequence(available);
      next = available + 1;
    }
  });

  const double start = Now();

  for (int64_t i = 0; i < iterations; ++i) {
    const int64_t sequence = sequencer.Claim();
    sequencer[sequence].value = sequence;
    sequencer.Publish(sequence);
  }

  // Wait until the consumer caught up with the last published sequence.
  while (consumer_sequence.sequence() < last_sequence) {
  }

  const double end = Now();
  consumer.join();

  std::cout.precision(15);
  std::cout << "1P-1EP-UNICAST performance: "
            << (iterations * 1.0) / (end - start) << " ops/secs" << std::endl;
  // The checksum of 0..iterations-1 is iterations*(iterations-1)/2, it doubles
  // as a proof that every event was really consumed.
  std::cout << "  buffer size: " << kBufferSize << ", events: " << iterations
            << ", checksum: " << checksum << std::endl;

  return EXIT_SUCCESS;
}
