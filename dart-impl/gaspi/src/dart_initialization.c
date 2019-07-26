#include <dash/dart/gaspi/dart_mem.h>
#include <dash/dart/gaspi/dart_team_private.h>
#include <dash/dart/gaspi/dart_gaspi.h>
#include <dash/dart/gaspi/dart_translation.h>
#include <dash/dart/gaspi/dart_communication_priv.h>

#include <dash/dart/base/locality.h>

#include <limits.h>

gaspi_rank_t dart_gaspi_rank_num;
gaspi_rank_t dart_gaspi_rank;

/**************** global auxiliary memory ****************/
/**
 * for internal communication
 */
gaspi_pointer_t dart_gaspi_buffer_ptr;
const gaspi_segment_id_t dart_gaspi_buffer_id = 0;
/*********************************************************/

/***************** non-collective memory *****************/
#define DART_BUDDY_ORDER 24
#define DART_LOCAL_ALLOC_SIZE (1024UL*1024*16)

// 10 fixed id's for special purposes
/* Gaspi segment number for non-collective memory */
const gaspi_segment_id_t dart_mempool_seg_localalloc = 1;
/* Point to the base address of memory region for local allocation. */
char * dart_mempool_localalloc;
/* Help to do memory management work for local allocation/free */
struct dart_buddy* dart_localpool;
/******************* collective memory *******************/
const gaspi_segment_id_t dart_coll_seg = 2;
const gaspi_segment_id_t dart_onesided_seg = 3;
bool dart_fallback_seg_is_allocated;
// segment to trigger remote completion with gaspi write
const gaspi_segment_id_t put_completion_src_seg = 4;
const gaspi_segment_id_t put_completion_dst_seg = 5;
char put_completion_dst_storage = PUT_COMPLETION_VALUE;

seg_stack_t pool_gaspi_seg_ids;
const gaspi_segment_id_t pool_gaspi_seg_ids_begin = 6;
const size_t pool_gaspi_seg_ids_count = 245;

seg_stack_t pool_dart_seg_ids;
// id 0 is reserved for non collective memory
const gaspi_segment_id_t pool_dart_seg_ids_begin = 1;
const size_t pool_dart_seg_ids_count = 254;
/*********************************************************/
static int _init_by_dart = 0;
static int _dart_initialized = 0;

dart_ret_t dart_init(int *argc, char ***argv)
{
    gaspi_return_t ret = gaspi_proc_init(GASPI_BLOCK);

    if(ret == GASPI_SUCCESS)
    {
        _init_by_dart = 1;
    }

    DART_CHECK_ERROR(gaspi_proc_rank(&dart_gaspi_rank));
    DART_CHECK_ERROR(gaspi_proc_num(&dart_gaspi_rank_num));

    /* Initialize the teamlist. */
    dart_adapt_teamlist_init();

    /* Create a global translation table for all the collective global memory */
    dart_adapt_transtable_create();

    datatype_init();

    dart_next_availteamid = DART_TEAM_ALL;
    gaspi_group_id_top = 0;

    uint16_t index;
    int result = dart_adapt_teamlist_alloc(DART_TEAM_ALL, &index);
    if (result == -1)
    {
        return DART_ERR_OTHER;
    }

    dart_teams[index].id = GASPI_GROUP_ALL;
    DART_CHECK_ERROR(dart_group_create(&(dart_teams[index].group)));

    for(gaspi_rank_t r = 0; r < dart_gaspi_rank_num ; ++r)
    {
        dart_global_unit_t id;
        id.id = r;
        DART_CHECK_ERROR(dart_group_addmember(dart_teams[index].group, id));
    }

    dart_next_availteamid++;

    DART_CHECK_ERROR(gaspi_segment_create(put_completion_src_seg,
                                          sizeof(char),
                                          GASPI_GROUP_ALL,
                                          GASPI_BLOCK,
                                          GASPI_MEM_INITIALIZED));

    DART_CHECK_ERROR(gaspi_segment_bind(put_completion_dst_seg, &put_completion_dst_storage, sizeof(put_completion_dst_storage), 0));
    /*
     * non-collective memory initialization
     */

    
    _dart_initialized = 1;
    DART_LOG_DEBUG("dart_init: put_completion_src_seg createt, put_completion_dst_seg bound to put_completion_dst_storage");

    dart_localpool = dart_buddy_new (DART_LOCAL_ALLOC_SIZE);

    DART_CHECK_ERROR(gaspi_segment_create(dart_mempool_seg_localalloc,
                                          DART_LOCAL_ALLOC_SIZE,
                                          GASPI_GROUP_ALL,
                                          GASPI_BLOCK,
                                          GASPI_MEM_INITIALIZED));
    gaspi_pointer_t seg_ptr;
    DART_CHECK_ERROR(gaspi_segment_ptr(dart_mempool_seg_localalloc, &seg_ptr));
    dart_mempool_localalloc = (char *) seg_ptr;

    DART_CHECK_ERROR(inital_rma_request_entry(0));
    /*
     * global auxiliary memory segement per process
     * for internal communication and dart collective operations
     */
    DART_CHECK_ERROR(gaspi_segment_create(dart_gaspi_buffer_id,
                                          DART_GASPI_BUFFER_SIZE,
                                          GASPI_GROUP_ALL,
                                          GASPI_BLOCK,
                                          GASPI_MEM_INITIALIZED));

    DART_CHECK_ERROR(gaspi_segment_ptr(dart_gaspi_buffer_id, &dart_gaspi_buffer_ptr));
    /*
     * Initializes free gaspi segment ids for collective memory and temporarily memory bindings
     */
    DART_CHECK_ERROR(seg_stack_init(&pool_gaspi_seg_ids, pool_gaspi_seg_ids_begin, pool_gaspi_seg_ids_count));

    /*
     * Initializes free dart segment ids for collective memory
     */
    DART_CHECK_ERROR(seg_stack_init(&pool_dart_seg_ids, pool_dart_seg_ids_begin, pool_dart_seg_ids_count));

    /*
     * dart fallback segment is not allocated at default
     */
    dart_fallback_seg_is_allocated = false;

    DART_LOG_DEBUG("dart_init: communication backend initialization finished");
    _dart_initialized = 2;

// switch on off
#ifdef WITHLOCALITY
    DART_LOG_DEBUG("ENABELING DART BASE LOCALITY");
    dart__base__locality__init();
#endif
    DART_LOG_DEBUG("dart_init > initialization finished");
    _dart_initialized = 3;

    return DART_OK;
}

dart_ret_t dart_exit()
{
    DART_CHECK_ERROR(gaspi_barrier(GASPI_GROUP_ALL, GASPI_BLOCK));

    DART_CHECK_ERROR(gaspi_segment_delete(dart_gaspi_buffer_id));

    DART_CHECK_ERROR(gaspi_segment_delete(dart_mempool_seg_localalloc));

    DART_CHECK_ERROR(gaspi_segment_delete(put_completion_src_seg));
    DART_CHECK_ERROR(gaspi_segment_delete(put_completion_dst_seg));

    uint16_t index;
    int result = dart_adapt_teamlist_convert(DART_TEAM_ALL, &index);
    if (result == -1)
    {
        return DART_ERR_INVAL;
    }

    DART_CHECK_ERROR(dart_group_destroy(&(dart_teams[index].group)));

    dart_buddy_delete(dart_localpool);

    DART_CHECK_ERROR(dart_adapt_transtable_destroy());

    DART_CHECK_ERROR(dart_adapt_teamlist_destroy());

    DART_CHECK_ERROR(seg_stack_finish(&pool_gaspi_seg_ids));

    DART_CHECK_ERROR(seg_stack_finish(&pool_dart_seg_ids));

    datatype_fini();
#ifdef WITHLOCALITY
    dart__base__locality__finalize();
#endif
    printf("dart_exit: finishes dart\n");
    if(_init_by_dart)
    {
       DART_CHECK_ERROR(gaspi_proc_term(GASPI_BLOCK));
    }
    return DART_OK;
}

void dart_abort(int errorcode)
{
  //DART_LOG_INFO("dart_abort: aborting DART run with error code %i", errorcode);
  gaspi_rank_t myRank;
  gaspi_proc_rank(&myRank);
  gaspi_proc_kill(myRank, GASPI_BLOCK);
  /* just in case gaspi_proc_kill does not abort process*/
  abort();
}

bool dart_initialized()
{
  return (_dart_initialized > 0);
}