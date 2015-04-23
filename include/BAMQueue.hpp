#ifndef __BAMQUEUE_HPP__
#define __BAMQUEUE_HPP__

//#include <boost/lockfree/spsc_queue.hpp>
//#include <boost/lockfree/queue.hpp>
#include <tbb/atomic.h>
#include <iostream>
#include <fstream>
#include <atomic>
#include <vector>
#include <memory>
#include <exception>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <boost/timer/timer.hpp>
#include <boost/filesystem.hpp>
#include <tbb/concurrent_queue.h>
#include "AlignmentGroup.hpp"
#include "LibraryFormat.hpp"
#include "SailfishMath.hpp"
#include "ReadPair.hpp"
#include "UnpairedRead.hpp"
#include "spdlog/spdlog.h"
#include "concurrentqueue.h"
#include "readerwriterqueue.h"

extern "C" {
#include "io_lib/scram.h"
#include "io_lib/os.h"
#undef max
#undef min
}

/**
  * Simple structure holding info about the alignment file.
  */
struct AlignmentFile {
    boost::filesystem::path fileName;
    std::string readMode;
    scram_fd* fp;
    SAM_hdr* header;
    uint32_t numParseThreads;
};

/**
  * A queue from which to draw BAM alignments.  The queue is thread-safe, and
  * can be written to and read from multiple threads.
  *
  * This class is templated on LibT --- the type of read library from which
  * the provided alignments are being generated.
  */
template <typename FragT>
class BAMQueue {
public:
  BAMQueue(std::vector<boost::filesystem::path>& fnames, LibraryFormat& libFmt,
           uint32_t numParseThreads, uint32_t cacheSize);
  ~BAMQueue();
  void forceEndParsing();

  SAM_hdr* header();
  SAM_hdr* safeHeader();

  std::vector<SAM_hdr*> headers();

  template <typename FilterT>
  void start(FilterT filt, bool onlyProcessAmbiguousAlignments = false);

  inline bool getAlignmentGroup(AlignmentGroup<FragT*>*& group);

  // Return the number of reads processed so far by the queue
  size_t numObservedReads();
  size_t numMappedReads();
  size_t numUniquelyMappedReads();

  void reset();

  tbb::concurrent_queue<FragT*>& getFragmentQueue();
  //moodycamel::ConcurrentQueue<FragT*>& getFragmentQueue();

  //tbb::concurrent_bounded_queue<AlignmentGroup<FragT*>*>& getAlignmentGroupQueue();
  moodycamel::ConcurrentQueue<AlignmentGroup<FragT*>*>& getAlignmentGroupQueue();

private:
  size_t popNum{0};
  /** Fill the queue with the appropriate type of alignment
   * depending on the template paramater T
   */
  template <typename FilterT>
  void fillQueue_(FilterT, bool);

  /** Overload of getFrag_ for paired-end reads */
  template <typename FilterT>
  inline bool getFrag_(ReadPair& rpair, FilterT filt);
  /** Overload of getFrag_ for single-end reads */
  template <typename FilterT>
  inline bool getFrag_(UnpairedRead& sread, FilterT filt);

public:
  bool verbose=false;
private:
  std::vector<AlignmentFile> files_;
  std::string fname_;
  LibraryFormat libFmt_;

  std::vector<AlignmentFile>::iterator currFile_;
  scram_fd* fp_ = nullptr;
  SAM_hdr* hdr_ = nullptr;

  //htsFile* fp_ = nullptr;
  size_t totalReads_;
  size_t numUnaligned_;
  size_t numMappedReads_;
  size_t numUniquelyMappedReads_;
  tbb::concurrent_queue<FragT*> fragmentQueue_;
  //moodycamel::ConcurrentQueue<FragT*> fragmentQueue_;

  //tbb::concurrent_bounded_queue<AlignmentGroup<FragT*>*> alnGroupPool_;
  moodycamel::ConcurrentQueue<AlignmentGroup<FragT*>*> alnGroupPool_;

  //tbb::concurrent_bounded_queue<AlignmentGroup<FragT*>*> alnGroupQueue_;
  moodycamel::ReaderWriterQueue<AlignmentGroup<FragT*>*> alnGroupQueue_;

  /*
  boost::lockfree::spsc_queue<AlignmentGroup<FragT*>*,
                              boost::lockfree::capacity<65535>> alnGroupQueue_;
                              */
  volatile bool doneParsing_;
  volatile bool exhaustedAlnGroupPool_;
  std::unique_ptr<std::thread> parsingThread_;
  std::shared_ptr<spdlog::logger> logger_;

  size_t batchNum_;
  std::string readMode_;
/*
#if not defined(__APPLE__)
   std::mutex agMutex_;
    std::condition_variable workAvailable_;
#endif
*/
};

#include "BAMQueue.tpp"
#endif //__BAMQUEUE_HPP__
