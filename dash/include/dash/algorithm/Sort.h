#ifndef DASH__ALGORITHM__SORT_H
#define DASH__ALGORITHM__SORT_H

#include <algorithm>
#include <functional>
#include <future>
#include <iterator>
#include <type_traits>
#include <vector>

#include <dash/Array.h>
#include <dash/Exception.h>
#include <dash/Meta.h>
#include <dash/dart/if/dart.h>

#include <dash/algorithm/Copy.h>
#include <dash/algorithm/LocalRange.h>

#include <dash/internal/Logging.h>
#include <dash/util/Trace.h>

#ifdef DASH_ENABLE_PSTL
#include <tbb/task_scheduler_init.h>
#endif

#ifdef DOXYGEN
namespace dash {
/**
 * Sorts the elements in the range, defined by \c [begin, end) in ascending
 * order. The order of equal elements is not guaranteed to be preserved.
 *
 * Elements are sorted using operator<. Additionally, the elements must be
 * arithmetic, i.e. std::is_arithmetic<T> must be satisfied.
 *
 * In terms of data distribution, source and destination ranges passed to
 * \c dash::sort must be global (\c GlobIter<ValueType>).
 *
 * The operation is collective among the team of the owning dash container.
 *
 * Example:
 *
 * \code
 *       dash::Array<int> arr(100);
 *       dash::generate(arr.begin(), arr.end());
 *       dash::sort(array.begin(),
 *                  array.end();
 * \endcode
 *
 * \ingroup  DashAlgorithms
 */
template <class GlobRandomIt>
void sort(GlobRandomIt begin, GlobRandomIt end);

/**
 * Sorts the elements in the range, defined by \c [begin, end) in ascending
 * order. The order of equal elements is not guaranteed to be preserved.
 *
 * Elements are sorted by using a user-defined hash function. Resulting values
 * of the hash function are required to be sortable by operator<.
 *
 * This variant may be appropriate if the underlying container does not hold
 * arithmetic values (e.g. structs).
 *
 * In terms of data distribution, source and destination ranges passed to
 * \c dash::sort must be global (\c GlobIter<ValueType>).
 *
 * The operation is collective among the team of the owning dash container.
 *
 * Example:
 *
 * \code
 *       struct pair { int x; int y; };
 *       dash::Array<pair> arr(100);
 *       dash::generate(arr.begin(), arr.end(), random());
 *       dash::sort(array.begin(),
 *                  array.end(),
 *                  [](pair const & p){return p.x % 13; }
 *                 );
 * \endcode
 *
 * \ingroup  DashAlgorithms
 */
template <class GlobRandomIt, class SortableHash>
void sort(GlobRandomIt begin, GlobRandomIt end, SortableHash hash);

}  // namespace dash

#else

#include <dash/algorithm/sort/Communication.h>
#include <dash/algorithm/sort/Histogram.h>
#include <dash/algorithm/sort/Partition.h>
#include <dash/algorithm/sort/Sort-inl.h>
#include <dash/algorithm/sort/Types.h>

namespace dash {

#define __DASH_SORT__FINAL_STEP_BY_MERGE (0)
#define __DASH_SORT__FINAL_STEP_BY_SORT (1)
#define __DASH_SORT__FINAL_STEP_STRATEGY (__DASH_SORT__FINAL_STEP_BY_MERGE)

template <class GlobRandomIt, class SortableHash>
void sort(GlobRandomIt begin, GlobRandomIt end, SortableHash sortable_hash)
{
  using iter_type  = GlobRandomIt;
  using value_type = typename iter_type::value_type;
  using mapped_type =
      typename std::decay<typename dash::functional::closure_traits<
          SortableHash>::result_type>::type;

  static_assert(
      std::is_arithmetic<mapped_type>::value,
      "Only arithmetic types are supported");

  auto pattern = begin.pattern();

  dash::util::Trace trace("Sort");

  auto const sort_comp = [&sortable_hash](
                             const value_type& a, const value_type& b) {
    return sortable_hash(a) < sortable_hash(b);
  };

  // Number of threads
  auto                     parallelism = 1;

#ifdef DASH_ENABLE_PSTL
  dash::util::TeamLocality tloc{pattern.team()};
  auto                     uloc = tloc.unit_locality(pattern.team().myid());
  parallelism                   = uloc.num_domain_threads();

  if (parallelism > 1) {
    // Initialize the scheduler with a specific number of threads
    // This is for example useful if we have one unit per NUMA_domain

    // This setting keeps fixed until the exit of the sorting algorithm
    tbb::task_scheduler_init init{parallelism};
  }
#endif

  if (pattern.team() == dash::Team::Null()) {
    DASH_LOG_TRACE("dash::sort", "Sorting on dash::Team::Null()");
    return;
  }
  if (pattern.team().size() == 1) {
    DASH_LOG_TRACE("dash::sort", "Sorting on a team with only 1 unit");
    trace.enter_state("1: final_local_sort");
    impl::local_sort(begin.local(), end.local(), sort_comp, parallelism);
    trace.exit_state("final_local_sort");
    return;
  }

  if (begin >= end) {
    DASH_LOG_TRACE("dash::sort", "empty range");
    trace.enter_state("1: final_barrier");
    pattern.team().barrier();
    trace.exit_state("1: final_barrier");
    return;
  }

  dash::Team& team   = pattern.team();
  auto const  nunits = team.size();
  auto const  myid   = team.myid();

  auto const unit_at_begin = pattern.unit_at(begin.pos());

  // local distance
  auto const l_range = dash::local_index_range(begin, end);

  auto* l_mem_begin = dash::local_begin(
      static_cast<typename GlobRandomIt::pointer>(begin), team.myid());

  auto const n_l_elem = l_range.end - l_range.begin;

  auto* lbegin = l_mem_begin + l_range.begin;
  auto* lend   = l_mem_begin + l_range.end;

  // initial local_sort
  trace.enter_state("1:initial_local_sort");
  impl::local_sort(lbegin, lend, sort_comp, parallelism);
  trace.exit_state("1:initial_local_sort");

  trace.enter_state("2:find_global_min_max");

  std::array<mapped_type, 2> min_max_in{
      // local minimum
      (n_l_elem > 0) ? sortable_hash(*lbegin)
                     : std::numeric_limits<mapped_type>::max(),
      (n_l_elem > 0) ? sortable_hash(*(std::prev(lend)))
                     : std::numeric_limits<mapped_type>::min()};

  std::array<mapped_type, 2> min_max_out{};

  DASH_ASSERT_RETURNS(
      dart_allreduce(
          &min_max_in,                              // send buffer
          &min_max_out,                             // receive buffer
          2,                                        // buffer size
          dash::dart_datatype<mapped_type>::value,  // data type
          DART_OP_MINMAX,                           // operation
          team.dart_id()                            // team
          ),
      DART_OK);

  auto const min_max = std::make_pair(
      min_max_out[DART_OP_MINMAX_MIN], min_max_out[DART_OP_MINMAX_MAX]);

  trace.exit_state("2:find_global_min_max");

  DASH_LOG_TRACE_VAR("global minimum in range", min_max.first);
  DASH_LOG_TRACE_VAR("global maximum in range", min_max.second);

  if (min_max.first == min_max.second) {
    // all values are equal, so nothing to sort globally.
    pattern.team().barrier();
    return;
  }

  trace.enter_state("3:init_temporary_local_data");

  std::vector<size_t> g_partition_data(nunits * 3);

  // Temporary local buffer (sorted);
  std::vector<value_type> lcopy(lbegin, lend);

  auto const p_unit_info =
      impl::psort__find_partition_borders(pattern, begin, end);

  auto const& acc_partition_count = p_unit_info.acc_partition_count;

  auto const nboundaries = nunits - 1;

  impl::Splitter<mapped_type> splitters(
      nboundaries, min_max.first, min_max.second);

  impl::psort__init_partition_borders(p_unit_info, splitters);

  DASH_LOG_TRACE_RANGE(
      "locally sorted array", std::begin(lcopy), std::end(lcopy));

  DASH_LOG_TRACE_RANGE(
      "skipped splitters",
      std::begin(splitters.threshold),
      std::end(splitters.threshold));

  bool done = false;

  // collect all valid splitters in a temporary vector
  std::vector<size_t> valid_partitions;

  {
    // make this as a separately scoped block to deallocate non-required
    // temporary memory
    std::vector<size_t> all_borders(nboundaries);
    std::iota(all_borders.begin(), all_borders.end(), 0);

    std::copy_if(
        all_borders.begin(),
        all_borders.end(),
        std::back_inserter(valid_partitions),
        [& is_skipped = splitters.is_skipped](size_t idx) {
          return is_skipped[idx] == false;
        });
  }

  DASH_LOG_TRACE_RANGE(
      "valid partitions",
      std::begin(valid_partitions),
      std::end(valid_partitions));

  if (valid_partitions.empty()) {
    // Edge case: We may have a team spanning at least 2 units, however the
    // global range is owned by  only 1 unit
    team.barrier();
    return;
  }

  size_t iter = 0;

  std::vector<size_t> global_histo(nunits * NLT_NLE_BLOCK, 0);

  trace.exit_state("3:init_temporary_local_data");

  trace.enter_state("4:find_global_partition_borders");

  do {
    ++iter;

    impl::psort__calc_boundaries(splitters);

    DASH_LOG_TRACE_VAR("finding partition borders", iter);

    DASH_LOG_TRACE_RANGE(
        "splitters",
        std::begin(splitters.threshold),
        std::end(splitters.threshold));

    auto const l_nlt_nle = impl::psort__local_histogram(
        splitters,
        valid_partitions,
        std::begin(lcopy),
        std::end(lcopy),
        sortable_hash);

    DASH_LOG_TRACE_RANGE(
        "local histogram ( < )",
        impl::make_strided_iterator(std::begin(l_nlt_nle)),
        impl::make_strided_iterator(std::begin(l_nlt_nle)) + nunits);

    DASH_LOG_TRACE_RANGE(
        "local histogram ( <= )",
        impl::make_strided_iterator(std::begin(l_nlt_nle) + 1),
        impl::make_strided_iterator(std::begin(l_nlt_nle) + 1) + nunits);

    // allreduce with implicit barrier
    impl::psort__global_histogram(
        // first partition
        std::begin(l_nlt_nle),
        // iterator past last valid partition
        std::next(
            std::begin(l_nlt_nle),
            (valid_partitions.back() + 1) * NLT_NLE_BLOCK),
        std::begin(global_histo),
        team.dart_id());

    DASH_LOG_TRACE_RANGE(
        "global histogram",
        std::next(std::begin(global_histo), myid * NLT_NLE_BLOCK),
        std::next(std::begin(global_histo), (myid + 1) * NLT_NLE_BLOCK));

    done = impl::psort__validate_partitions(
        p_unit_info, splitters, valid_partitions, global_histo);
  } while (!done);

  trace.exit_state("4:find_global_partition_borders");

  DASH_LOG_TRACE_VAR("partition borders found after N iterations", iter);

  /********************************************************************/
  /****** Final Histogram *********************************************/
  /********************************************************************/

  trace.enter_state("5:final_local_histogram");

  /* How many elements are less than P
   * or less than equals P */
  auto const histograms = impl::psort__local_histogram(
      splitters,
      valid_partitions,
      std::begin(lcopy),
      std::end(lcopy),
      sortable_hash);

  trace.exit_state("5:final_local_histogram");

  DASH_LOG_TRACE_RANGE(
      "final splitters",
      std::begin(splitters.threshold),
      std::end(splitters.threshold));

  DASH_LOG_TRACE_RANGE(
      "local histogram ( < )",
      impl::make_strided_iterator(std::begin(histograms)),
      impl::make_strided_iterator(std::begin(histograms)) + nunits);

  DASH_LOG_TRACE_RANGE(
      "local histogram ( <= )",
      impl::make_strided_iterator(std::begin(histograms) + 1),
      impl::make_strided_iterator(std::begin(histograms) + 1) + nunits);

  /********************************************************************/
  /****** Partition Distribution **************************************/
  /********************************************************************/

  trace.enter_state("6:transpose_local_histograms (all-to-all)");

  DASH_ASSERT_RETURNS(
      dart_alltoall(
          // send buffer
          histograms.data(),
          // receive buffer
          g_partition_data.data(),
          // we send / receive 1 element to / from each process
          NLT_NLE_BLOCK,
          // dtype
          dash::dart_datatype<size_t>::value,
          // teamid
          team.dart_id()),
      DART_OK);

  DASH_LOG_TRACE_RANGE(
      "initial partition distribution",
      impl::make_strided_iterator(std::begin(g_partition_data)),
      impl::make_strided_iterator(std::begin(g_partition_data)) + nunits);

  DASH_LOG_TRACE_RANGE(
      "initial partition supply",
      impl::make_strided_iterator(std::begin(g_partition_data) + 1),
      impl::make_strided_iterator(std::begin(g_partition_data) + 1) + nunits);

  trace.exit_state("6:transpose_local_histograms (all-to-all)");

  /* Calculate final distribution per partition. Each unit is responsible for
   * its own bucket.
   */

  trace.enter_state("7:calc_final_partition_dist");

  auto first_nlt = impl::make_strided_iterator(std::begin(g_partition_data));

  auto first_nle =
      impl::make_strided_iterator(std::next(std::begin(g_partition_data)));

  impl::psort__calc_final_partition_dist(
      first_nlt,
      first_nlt + nunits,
      first_nle,
      acc_partition_count[myid + 1]);

  // let us now collapse the data into a contiguous range with unit stride
  std::move(
      impl::make_strided_iterator(std::begin(g_partition_data)) + 1,
      impl::make_strided_iterator(std::begin(g_partition_data)) + nunits,
      std::next(std::begin(g_partition_data)));

  DASH_LOG_TRACE_RANGE(
      "final partition distribution",
      std::next(std::begin(g_partition_data), IDX_DIST(nunits)),
      std::next(std::begin(g_partition_data), IDX_DIST(nunits) + nunits));

  trace.exit_state("7:calc_final_partition_dist");

  /********************************************************************/
  /****** Target Distribution *****************************************/
  /********************************************************************/

  trace.enter_state("8:transpose_final_partition_dist (all-to-all)");

  DASH_ASSERT_RETURNS(
      dart_alltoall(
          // send buffer
          std::next(g_partition_data.data(), IDX_DIST(nunits)),
          // receive buffer
          std::next(g_partition_data.data(), IDX_TARGET_COUNT(nunits)),
          // we send / receive 1 element to / from each process
          1,
          // dtype
          dash::dart_datatype<size_t>::value,
          // teamid
          team.dart_id()),
      DART_OK);

  DASH_LOG_TRACE_RANGE(
      "target distribution",
      std::next(std::begin(g_partition_data), IDX_TARGET_COUNT(nunits)),
      std::next(
          std::begin(g_partition_data), IDX_TARGET_COUNT(nunits) + nunits));

  trace.exit_state("8:transpose_final_partition_dist (all-to-all)");

  /********************************************************************/
  /****** Source Count ************************************************/
  /********************************************************************/

  trace.enter_state("9:calc_final_send_count");

  auto l_send_count =
      std::next(std::begin(g_partition_data), IDX_SRC_COUNT(nunits));

  if (n_l_elem > 0) {
    auto const l_target_count =
        std::next(std::begin(g_partition_data), IDX_TARGET_COUNT(nunits));

    impl::psort__calc_send_count(
        splitters, valid_partitions, l_target_count, l_send_count);
  }
  else {
    std::fill(
        std::next(std::begin(g_partition_data), IDX_SRC_COUNT(nunits)),
        std::next(
            std::begin(g_partition_data), IDX_SRC_COUNT(nunits) + nunits),
        0);
  }

  DASH_LOG_TRACE_RANGE(
      "source count",
      std::next(std::begin(g_partition_data), IDX_SRC_COUNT(nunits)),
      std::next(
          std::begin(g_partition_data), IDX_SRC_COUNT(nunits) + nunits));

  trace.exit_state("9:calc_final_send_count");

  /********************************************************************/
  /****** Target Count ************************************************/
  /********************************************************************/

  auto* l_target_count =
      std::next(g_partition_data.data(), IDX_TARGET_COUNT(nunits));

  DASH_ASSERT_RETURNS(
      dart_alltoall(
          // send buffer
          std::next(g_partition_data.data(), IDX_SRC_COUNT(nunits)),
          // receive buffer
          l_target_count,
          // we send / receive 1 element to / from each process
          1,
          // dtype
          dash::dart_datatype<size_t>::value,
          // teamid
          team.dart_id()),
      DART_OK);

  auto l_target_displs =
      std::next(g_partition_data.data(), IDX_SRC_COUNT(nunits));

  *l_target_displs = 0;

  // exclusive scan using partial sum
  std::partial_sum(
      l_target_count,
      std::next(l_target_count, nunits - 1),
      std::next(l_target_displs),
      std::plus<size_t>());

#if defined(DASH_ENABLE_ASSERTIONS) && defined(DASH_ENABLE_TRACE_LOGGING)
  DASH_ASSERT_EQ(
      std::accumulate(
          l_target_count, l_target_count + nunits, std::size_t{0}),
      n_l_elem,
      "invalid target count");
#endif

  DASH_LOG_TRACE_RANGE(
      "target count", l_target_count, l_target_count + nunits);

  DASH_LOG_TRACE_RANGE(
      "target displs", l_target_displs, std::next(l_target_displs, nunits));

  /********************************************************************/
  /****** Source Displs ***********************************************/
  /********************************************************************/

  trace.enter_state("10:calc_final_target_displs");

  auto l_src_displs =
      std::next(std::begin(g_partition_data), IDX_DISP(nunits));

  dash::exclusive_scan(
      // first
      std::next(std::begin(g_partition_data), IDX_TARGET_COUNT(nunits)),
      // last
      std::next(
          std::begin(g_partition_data), IDX_TARGET_COUNT(nunits) + nunits),
      // out
      std::addressof(*l_src_displs),
      // init
      std::size_t{0},
      // op
      dash::plus<std::size_t>{},
      // team
      team);

  if (!myid) {
    std::fill(l_src_displs, std::next(l_src_displs, nunits), 0);
  }

  DASH_LOG_TRACE_RANGE("source displs", l_src_displs, l_src_displs + nunits);

  trace.exit_state("10:calc_final_target_displs");

  trace.enter_state("11:exchange_data (all-to-all)");

  std::vector<dash::Future<iter_type> > async_copies{};
  async_copies.reserve(p_unit_info.valid_remote_partitions.size());

  auto const get_send_info = [&g_partition_data, &l_target_displs, nunits](
                                 dash::default_index_t const p_idx) {
    auto const target_disp = l_target_displs[p_idx];
    auto const target_count =
        g_partition_data[p_idx + IDX_TARGET_COUNT(nunits)];
    auto const src_disp = g_partition_data[p_idx + IDX_DISP(nunits)];
    return std::make_tuple(target_count, src_disp, target_disp);
  };

  std::size_t target_count, src_disp, target_disp;

  // A range of chunks to be merged.
  using chunk_range_t = std::pair<std::size_t, std::size_t>;
  // Futures for the merges - only used to signal readiness.
  // Use a std::map because emplace will not invalidate any
  // references or iterators.
  std::map<chunk_range_t, std::future<void> > merge_dependencies;

  for (auto const& unit : p_unit_info.valid_remote_partitions) {
    std::tie(target_count, src_disp, target_disp) = get_send_info(unit);

    if (0 == target_count) {
      continue;
    }

    DASH_LOG_TRACE(
        "async copy",
        "source unit",
        unit,
        "target_count",
        target_count,
        "src_disp",
        src_disp,
        "target_disp",
        target_disp);

    // Get a global iterator to the first local element of a unit within the
    // range to be sorted [begin, end)
    //
    iter_type it_src =
        (unit == unit_at_begin)
            ?
            /* If we are the unit at the beginning of the global range simply
               return begin */
            begin
            :
            /* Otherwise construct an global iterator pointing the first local
               element from the correspoding unit */
            iter_type{&(begin.globmem()),
                      pattern,
                      pattern.global_index(
                          static_cast<dash::team_unit_t>(unit), {})};

    // A chunk range (unit, unit + 1) signals represents the copy. Unit + 1 is
    // a sentinel here.
    chunk_range_t unit_range(unit, unit + 1);
    auto&&        fut = dash::copy_async(
        it_src + src_disp,
        it_src + src_disp + target_count,
        std::addressof(*(lcopy.begin() + target_disp)));

    // The std::async is necessary to convert to std::future<void>
    merge_dependencies.emplace(
        unit_range,
        std::async(std::launch::async, [f = std::move(fut)]() mutable {
          f.wait();
        }));
  }

  std::tie(target_count, src_disp, target_disp) = get_send_info(myid);

  // Create an entry for the local part
  chunk_range_t local_range(myid, myid + 1);
  merge_dependencies[local_range] = std::async(
      std::launch::async,
      [target_count, local_range, src_disp, &lcopy, target_disp, lbegin] {
        if (target_count) {
          std::copy(
              std::next(lbegin, src_disp),
              std::next(lbegin, src_disp + target_count),
              std::next(std::begin(lcopy), target_disp));
        }
      });

  trace.exit_state("11:exchange_data (all-to-all)");

  /* NOTE: While merging locally sorted sequences is faster than another
   * heavy-weight sort it comes at a cost. std::inplace_merge allocates a
   * temporary buffer internally which is also documented on cppreference. If
   * the allocation of this buffer fails, a less efficient merge method is
   * used. However, in Linux, the allocation nevers fails since the
   * implementation simply allocates memory using malloc and the kernel
   * follows the optimistic strategy. This is ugly and can lead to a
   * segmentation fault later if no physical pages are available to map the
   * allocated virtual memory.
   *
   *
   * std::sort does not suffer from this problem and may be a more safe
   * variant, especially if the user wants to utilize the fully available
   * memory capacity on its own.
   */

#if (__DASH_SORT__FINAL_STEP_STRATEGY == __DASH_SORT__FINAL_STEP_BY_SORT)
  trace.enter_state("12:barrier");
  team.barrier();
  trace.exit_state("12:barrier");

  trace.enter_state("13:final_local_sort");
  impl::local_sort(lbegin, lend, sort_comp, parallelism);
  trace.exit_state("13:final_local_sort");
#else
  trace.enter_state("13:merge_local_sequences");

  // merging sorted sequences
  auto nsequences = nunits;

  // number of merge steps in the tree
  auto const depth = static_cast<size_t>(std::ceil(std::log2(nsequences)));

  // calculate the prefix sum among all receive counts to find the offsets for
  // merging
  std::vector<size_t> recv_count_psum;
  recv_count_psum.reserve(nsequences + 1);
  recv_count_psum.emplace_back(0);

  std::partial_sum(
      l_target_count,
      l_target_count + nunits,
      std::back_inserter(recv_count_psum));

  DASH_LOG_TRACE_RANGE(
      "recv count prefix sum",
      std::begin(recv_count_psum),
      std::end(recv_count_psum));

  DASH_LOG_TRACE_RANGE("before merging", lcopy.begin(), lcopy.end());

  for (std::size_t d = 0; d < depth; ++d) {
    // distance between first and mid iterator while merging
    auto const step = std::size_t(0x1) << d;
    // distance between first and last iterator while merging
    auto const dist = step << 1;
    // number of merges
    auto const nmerges = nsequences >> 1;

    // Start threaded merges. When d == 0 they depend on dash::copy to finish,
    // later on other merges.
    for (std::size_t m = 0; m < nmerges; ++m) {
      auto f  = m * dist;
      auto mi = m * dist + step;
      // sometimes we have a lonely merge in the end, so we have to guarantee
      // that we do not access out of bounds
      auto          l = std::min(m * dist + dist, recv_count_psum.size() - 1);
      auto          first = std::next(lcopy.begin(), recv_count_psum[f]);
      auto          mid   = std::next(lcopy.begin(), recv_count_psum[mi]);
      auto          last  = std::next(lcopy.begin(), recv_count_psum[l]);
      chunk_range_t dep_l(f, mi);
      chunk_range_t dep_r(mi, l);

      // Start a thread that blocks until the two previous merges are ready.
      auto&& fut = std::async(
          std::launch::async,
          [nunits,
           lbegin,
           first,
           mid,
           last,
           dep_l,
           dep_r,
           &team,
           &merge_dependencies]() {
            if (merge_dependencies.count(dep_l)) {
              merge_dependencies[dep_l].wait();
            }
            if (merge_dependencies.count(dep_r)) {
              merge_dependencies[dep_r].wait();
            }

            // The final merge can be done non-inplace, because we need to
            // copy the result to the final buffer anyways.
            if (dep_l.first == 0 && dep_r.second == nunits) {
              // Make sure everyone merged their parts (necessary for the copy
              // into the final buffer)
              team.barrier();
              std::merge(first, mid, mid, last, lbegin);
            }
            else {
              std::inplace_merge(first, mid, last);
            }
            DASH_LOG_TRACE("merged chunks", dep_l.first, dep_r.second);
          });
      chunk_range_t to_merge(f, l);
      merge_dependencies.emplace(to_merge, std::move(fut));
    }

    nsequences -= nmerges;
  }

  // Wait for the final merge step
  chunk_range_t final_range(0, nunits);
  merge_dependencies.at(final_range).wait();

  trace.exit_state("13:merge_local_sequences");
#endif

  DASH_LOG_TRACE_RANGE("finally sorted range", lbegin, lend);

  trace.enter_state("14:final_barrier");
  team.barrier();
  trace.exit_state("14:final_barrier");
}

namespace impl {
template <typename T>
struct identity_t : std::unary_function<T, T> {
  constexpr T&& operator()(T&& t) const noexcept
  {
    // A perfect forwarding identity function
    return std::forward<T>(t);
  }
};
}  // namespace impl

template <class GlobRandomIt>
inline void sort(GlobRandomIt begin, GlobRandomIt end)
{
  using value_t = typename std::remove_cv<
      typename dash::iterator_traits<GlobRandomIt>::value_type>::type;

  dash::sort(begin, end, impl::identity_t<value_t const&>());
}

#endif  // DOXYGEN

}  // namespace dash

#endif  // DASH__ALGORITHM__SORT_Hll
