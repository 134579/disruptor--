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

// One stage of the pipeline: it waits on its own barrier, walks every event of
// the batch it was granted and finally publishes its own progress.
void Consume(SequenceBarrier<kDefaultWaitStrategy>& barrier,
             BenchmarkSequencer& sequencer, Sequence& stage_sequence,
             const int64_t& last_sequence, int64_t* checksum) {
  int64_t next = kFirstSequenceValue;
  while (next <= last_sequence) {
    const int64_t available = barrier.WaitFor(next);
    if (available < next) continue;  // alerted, cannot happen here

    for (int64_t sequence = next; sequence <= available; ++sequence) {
      *checksum += sequencer[sequence].value;
    }

    stage_sequence.set_sequence(available);
    next = available + 1;
  }
}

}  // namespace

int main(int argc, char** argv) {
  const int64_t iterations = argc > 1 ? std::atol(argv[1]) : kDefaultIterations;
  // The last sequence that will ever be published, sequences start at 0.
  const int64_t last_sequence = iterations - 1;

  std::array<StubEvent, kBufferSize> events{};
  BenchmarkSequencer sequencer(events);

  Sequence first_sequence;
  Sequence second_sequence;
  Sequence third_sequence;

  // Gating the publisher on the last stage is enough: a stage can only be at
  // sequence S once every stage before it reached S, so third_sequence is
  // already the minimum of the three.
  sequencer.set_gating_sequences({&third_sequence});

  // Each stage gates on the previous one, the first one gates on the cursor.
  SequenceBarrier<kDefaultWaitStrategy> first_barrier =
      sequencer.NewBarrier({});
  SequenceBarrier<kDefaultWaitStrategy> second_barrier =
      sequencer.NewBarrier({&first_sequence});
  SequenceBarrier<kDefaultWaitStrategy> third_barrier =
      sequencer.NewBarrier({&second_sequence});

  int64_t first_checksum = 0;
  int64_t second_checksum = 0;
  int64_t third_checksum = 0;

  std::thread first([&]() {
    Consume(first_barrier, sequencer, first_sequence, last_sequence,
            &first_checksum);
  });
  std::thread second([&]() {
    Consume(second_barrier, sequencer, second_sequence, last_sequence,
            &second_checksum);
  });
  std::thread third([&]() {
    Consume(third_barrier, sequencer, third_sequence, last_sequence,
            &third_checksum);
  });

  const double start = Now();

  for (int64_t i = 0; i < iterations; ++i) {
    const int64_t sequence = sequencer.Claim();
    sequencer[sequence].value = sequence;
    sequencer.Publish(sequence);
  }

  // Wait until the last stage caught up with the last published sequence.
  while (third_sequence.sequence() < last_sequence) {
  }

  const double end = Now();

  first.join();
  second.join();
  third.join();

  std::cout.precision(15);
  std::cout << "1P-3EP-PIPELINE performance: "
            << (iterations * 1.0) / (end - start) << " ops/secs" << std::endl;
  // Every stage sums 0..iterations-1, the total doubles as a proof that each
  // event made it through all three of them exactly once.
  std::cout << "  buffer size: " << kBufferSize << ", events: " << iterations
            << ", checksum: "
            << (first_checksum + second_checksum + third_checksum) << std::endl;

  return EXIT_SUCCESS;
}
