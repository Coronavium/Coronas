#include <search.h>
#include <string.h>
#include <assert.h>

#include <dash/dart/gaspi/gaspi_utils.h>
#include <dash/dart/gaspi/dart_communication_priv.h>
#include <dash/dart/gaspi/dart_translation.h>

tree_root * rma_request_table[DART_MAX_SEGS];


typedef struct{
    dart_unit_t             key;
    gaspi_queue_id_t        queue;
}request_table_entry_t;

static void * request_table_key(struct stree_node * node)
{
    request_table_entry_t * tmp = (request_table_entry_t *) node->node;
    return (void *) &(tmp->key);
}

static int64_t compare_rma_requests(void* keyA, void* keyB)
{
    dart_unit_t u1 = ((request_table_entry_t *) keyA)->key;
    dart_unit_t u2 = ((request_table_entry_t *) keyB)->key;

    return u1 - u2;
}

dart_ret_t inital_rma_request_table()
{
    for(int i = 0; i < DART_MAX_SEGS; ++i)
    {
        rma_request_table[i] = NULL;
    }
    return DART_OK;
}

dart_ret_t inital_rma_request_entry(int16_t seg_id)
{
    rma_request_table[seg_id] = NULL;
    return DART_OK;
}

dart_ret_t delete_rma_requests(int16_t seg_id)
{
    if(rma_request_table[seg_id] != NULL)
    {
        destroy_rbtree(rma_request_table[seg_id]);
        rma_request_table[seg_id] = NULL;
    }
    return DART_OK;
}

dart_ret_t destroy_rma_request_table()
{
    for(int i = 0; i < DART_MAX_SEGS; ++i)
    {
        if(rma_request_table[i] != NULL)
        {
            destroy_rbtree(rma_request_table[i]);
        }
    }
    return DART_OK;
}
/**
 * Search for an existing entry
 * exists ->  returns the queue-id and set found to 1
 * Otherwise -> set found to 0
 */
dart_ret_t find_rma_request(dart_unit_t target_unit, int16_t seg_id, gaspi_queue_id_t * qid, char * found)
{
    if(rma_request_table[seg_id] != NULL)
    {
        request_table_entry_t * entry = (request_table_entry_t *) search_rbtree(*(rma_request_table[seg_id]), &target_unit);
        if(entry != NULL)
        {
            *qid = entry->queue;
            *found = 1;

            return DART_OK;
        }
    }
    *found = 0;
    return DART_OK;
}
/**
 * Before add an entry, check if the entry exists
 * because exisiting entries will be replaced
 */
dart_ret_t add_rma_request_entry(dart_unit_t target_unit, int16_t seg_id, gaspi_queue_id_t qid)
{
    if(rma_request_table[seg_id] == NULL)
    {
        rma_request_table[seg_id] = new_rbtree(request_table_key, compare_rma_requests);
    }
    request_table_entry_t * tmp_entry = (request_table_entry_t *) malloc(sizeof(request_table_entry_t));
    assert(tmp_entry);

    tmp_entry->key   = target_unit;
    tmp_entry->queue = qid;
    /*
     * TODO if the entry exists, it will be replaced !!
     * This call ships the owner of the pointer
     */
    void * existing_entry = rb_tree_insert(rma_request_table[seg_id], tmp_entry);
    if(existing_entry != NULL)
    {
        free(existing_entry);
    }

    return DART_OK;
}

request_iterator_t new_request_iter(int16_t seg_id)
{
    if(rma_request_table[seg_id] != NULL)
    {
        return new_tree_iterator(rma_request_table[seg_id]);
    }
    return NULL;
}

dart_ret_t destroy_request_iter(request_iterator_t iter)
{
    if(iter != NULL)
    {
        destroy_iterator(iter);
        return DART_OK;
    }

    return DART_ERR_INVAL;
}

uint8_t request_iter_is_vaild(request_iterator_t iter)
{
    if(iter != NULL)
    {
        return tree_iterator_has_next(iter);
    }
    return 0;
}

dart_ret_t request_iter_next(request_iterator_t iter)
{
    if(iter != NULL)
    {
        tree_iterator_next(iter);
        return DART_OK;
    }
    return DART_ERR_INVAL;
}

dart_ret_t request_iter_get_queue(request_iterator_t iter, gaspi_queue_id_t * qid)
{
    if(iter != NULL)
    {
        request_table_entry_t * entry = (request_table_entry_t *) tree_iterator_value(iter);
        *qid = entry->queue;
        return DART_OK;
    }
    return DART_ERR_INVAL;
}

dart_ret_t unit_l2g (uint16_t index, dart_unit_t *abs_id, dart_unit_t rel_id)
{
    if (index == 0)
    {
        *abs_id = rel_id;
    }
    else
    {
        *abs_id = dart_teams[index].group->l2g[rel_id];
    }

    return DART_OK;
}

dart_ret_t unit_g2l (uint16_t index, dart_unit_t abs_id, dart_unit_t *rel_id)
{
    if (index == 0)
    {
        *rel_id = abs_id;
    }
    else
    {
        *rel_id = dart_teams[index].group->g2l[abs_id];
    }

    return DART_OK;
}

int dart_gaspi_cmp_ranks(const void * a, const void * b)
{
    const int c = *((gaspi_rank_t *) a);
    const int d = *((gaspi_rank_t *) b);
    return c - d;
}

dart_ret_t dart_get_minimal_queue(gaspi_queue_id_t * qid)
{
    gaspi_number_t qsize;
    gaspi_number_t queue_num_max;
    gaspi_number_t min_queue_size;
    gaspi_number_t queue_size_max;

    DART_CHECK_ERROR(gaspi_queue_size_max(&queue_size_max));
    DART_CHECK_ERROR(gaspi_queue_num(&queue_num_max));

    min_queue_size = queue_size_max;

    for(gaspi_queue_id_t q = 0 ; q < queue_num_max ; ++q)
    {
        DART_CHECK_ERROR(gaspi_queue_size(q, &qsize));
        if(qsize == 0)
        {
            *qid = q;
            return DART_OK;
        }
        if(min_queue_size > qsize)
        {
            min_queue_size = qsize;
            *qid = q;
        }
    }
    /*
     * If there is no empty queue -> perform wait operation to empty one queue
     */
    if(min_queue_size == queue_size_max)
    {
        DART_CHECK_ERROR(gaspi_wait(*qid, GASPI_BLOCK));
    }

    return DART_OK;
}

dart_ret_t glob_unit_gaspi_seg(dart_gptr_t* gptr, dart_unit_t* global_unit_id, gaspi_segment_id_t* gaspi_seg_id, const char* location)
{
    if(gptr->segid)
    {
        //dart_team_unit_l2g(gptr.teamid, target_unit, &global_target_unit);
        DART_CHECK_ERROR(unit_l2g(gptr->flags, global_unit_id, gptr->unitid));
        if(dart_adapt_transtable_get_gaspi_seg_id(gptr->segid, gptr->unitid, gaspi_seg_id) == -1)
        {
            fprintf(stderr, "Can't find given segment id in %s\n", location);
            return DART_ERR_NOTFOUND;
        }
    }

    return DART_OK;
}


void set_multiple_block(converted_type_t* conv_type, size_t num_blocks)
{
    conv_type->num_blocks = num_blocks;
    conv_type->kind = DART_BLOCK_MULTIPLE;
    conv_type->multiple = (multiple_t){(offset_pair_t*) malloc(sizeof(offset_pair_t) * num_blocks),
                                        (size_t*) malloc(sizeof(size_t) * num_blocks)};
}

void set_single_block(converted_type_t* conv_type, size_t num_blocks, offset_pair_t offset_pair, size_t nbytes)
{
    conv_type->num_blocks = num_blocks;
    conv_type->kind = DART_BLOCK_SINGLE;
    conv_type->single = (single_t){offset_pair, nbytes};
}

void free_converted_type(converted_type_t* conv_type)
{
    if(conv_type->kind == DART_BLOCK_MULTIPLE)
    {
      if(conv_type->multiple.offsets != NULL)
        free(conv_type->multiple.offsets);
      if(conv_type->multiple.nbytes != NULL)
        free(conv_type->multiple.nbytes);
    }
}

dart_ret_t dart_convert_type(dart_datatype_struct_t* dts_src,
                             dart_datatype_struct_t* dts_dst,
                             size_t nelem,
                             converted_type_t* conv_type)
{
    size_t nbytes_elem = datatype_sizeof(datatype_base_struct(dts_src));

    if (datatype_iscontiguous(dts_src) && datatype_iscontiguous(dts_dst))
    {
        set_single_block(conv_type, 1, (offset_pair_t){0,0}, nelem * nbytes_elem);

        return DART_OK;
    }

    if(datatype_iscontiguous(dts_src) || datatype_iscontiguous(dts_dst))
    {
        if(datatype_iscontiguous(dts_src))
        {
            if(datatype_isstrided(dts_dst))
            {
                size_t num_blocks = nelem / dts_dst->num_elem;
                size_t num_elem_byte = dts_dst->num_elem * nbytes_elem;

                set_single_block(conv_type, num_blocks, (offset_pair_t){num_elem_byte,dts_dst->strided.stride * nbytes_elem}, num_elem_byte);

                return DART_OK;
            }

            if(datatype_isindexed(dts_dst))
            {
                size_t num_blocks = dts_dst->indexed.num_blocks;
                set_multiple_block(conv_type, num_blocks);
                size_t offset_src = 0;
                for(int i = 0; i < num_blocks; ++i)
                {
                    size_t num_elem_byte = dts_dst->indexed.blocklens[i] * nbytes_elem;
                    conv_type->multiple.nbytes[i] = num_elem_byte;
                    conv_type->multiple.offsets[i] = (offset_pair_t){offset_src, dts_dst->indexed.offsets[i] * nbytes_elem};
                    offset_src += num_elem_byte;
                }

                return DART_OK;
            }
        }
        else
        {
            if(datatype_isstrided(dts_src))
            {
                size_t num_blocks = nelem / dts_src->num_elem;
                size_t num_elem_byte = dts_src->num_elem * nbytes_elem;

                set_single_block(conv_type, num_blocks, (offset_pair_t){dts_src->strided.stride * nbytes_elem, num_elem_byte}, num_elem_byte);

                return DART_OK;
            }

            if(datatype_isindexed(dts_src))
            {
                size_t num_blocks = dts_src->indexed.num_blocks;
                set_multiple_block(conv_type, num_blocks);
                size_t offset_dst = 0;
                for(int i = 0; i < num_blocks; ++i)
                {
                    size_t num_elem_byte = dts_src->indexed.blocklens[i] * nbytes_elem;
                    conv_type->multiple.nbytes[i] = num_elem_byte;
                    conv_type->multiple.offsets[i] = (offset_pair_t){dts_src->indexed.offsets[i] * nbytes_elem, offset_dst};
                    offset_dst += num_elem_byte;
                }

                return DART_OK;
            }
        }

        return DART_ERR_INVAL;
    }

    // only strided and indexed datatypes should left
    if(!(datatype_isstrided(dts_src) || datatype_isindexed(dts_src)) &&
       !(datatype_isstrided(dts_dst) || datatype_isindexed(dts_dst)))
    {
        return DART_ERR_INVAL;
    }


    if(datatype_isstrided(dts_src) && datatype_isstrided(dts_dst) && dts_src->num_elem == dts_dst->num_elem)
    {
        size_t num_blocks = nelem / dts_src->num_elem;
        set_single_block(conv_type, num_blocks, (offset_pair_t){dts_src->strided.stride * nbytes_elem, dts_dst->strided.stride * nbytes_elem}, dts_src->num_elem * nbytes_elem);

        return DART_OK;
    }

    size_t nblocks_src = datatype_isstrided(dts_src) ? nelem / dts_src->num_elem : dts_src->indexed.num_blocks;
    size_t nblocks_dst = datatype_isstrided(dts_dst) ? nelem / dts_dst->num_elem : dts_dst->indexed.num_blocks;
    // simple solution -> allocates slightly more memory but
    // doesn't need to calculte the exact number of blocks (more complex)
    set_multiple_block(conv_type, nblocks_src + nblocks_dst);

    size_t block_src = 0;
    size_t block_dst = 0;

    size_t offset_src = datatype_isstrided(dts_src) ? 0 : dts_src->indexed.offsets[block_src];
    size_t offset_dst = datatype_isstrided(dts_dst) ? 0 : dts_dst->indexed.offsets[block_dst];
    size_t elems_src = datatype_isstrided(dts_src) ? dts_src->num_elem : dts_src->indexed.blocklens[block_src];
    size_t elems_dst = datatype_isstrided(dts_dst) ? dts_dst->num_elem : dts_dst->indexed.blocklens[block_dst];

    size_t elems_done = 0;
    size_t block_id = 0;

    do
    {
        size_t min_elem = MIN(elems_src, elems_dst);
        size_t nelem_byte = min_elem * nbytes_elem;
        conv_type->multiple.nbytes[block_id] = nelem_byte;
        conv_type->multiple.offsets[block_id] = (offset_pair_t){offset_src, offset_dst};

        elems_src -= min_elem;
        elems_dst -= min_elem;
        if(elems_src == 0)
        {
            ++block_src;
            if(datatype_isstrided(dts_src))
            {
                elems_src = dts_src->num_elem;
                offset_src = block_src * dts_src->strided.stride * nbytes_elem;
            }
            else
            {
                if(block_src < dts_src->indexed.num_blocks)
                {
                    elems_src = dts_src->indexed.blocklens[block_src];
                    offset_src = dts_src->indexed.offsets[block_src] * nbytes_elem;
                }
            }
        }
        else
        {
            offset_src += nelem_byte;
        }

        if(elems_dst == 0)
        {
            ++block_dst;
            if(datatype_isstrided(dts_dst))
            {
                elems_dst = dts_dst->num_elem;
                offset_dst = block_dst * dts_dst->strided.stride * nbytes_elem;
            }
            else
            {
                if(block_dst < dts_dst->indexed.num_blocks)
                {
                    elems_dst = dts_dst->indexed.blocklens[block_dst];
                    offset_dst = dts_dst->indexed.offsets[block_dst] * nbytes_elem;
                }
            }
        }
        else
        {
            offset_dst += nelem_byte;
        }
        elems_done += min_elem;
        ++block_id;

    } while (elems_done < nelem);

    conv_type->num_blocks = block_id;

    return DART_OK;
}

void local_copy_impl(char* src, char* dst, converted_type_t* conv_type)
{
    size_t offset_src = 0;
    size_t offset_dst = 0;
    size_t nbytes_read = 0;
    if(conv_type->kind == DART_BLOCK_SINGLE)
    {
        nbytes_read = conv_type->single.nbyte;
    }
    for(int i = 0; i < conv_type->num_blocks; ++i)
    {
      if(conv_type->kind == DART_BLOCK_MULTIPLE)
      {
        offset_src = conv_type->multiple.offsets[i].src;
        offset_dst = conv_type->multiple.offsets[i].dst;
        nbytes_read = conv_type->multiple.nbytes[i];
      }
      memcpy((void*)(dst + offset_dst), (void*)(src + offset_src), nbytes_read);

      if(conv_type->kind == DART_BLOCK_SINGLE)
      {
        offset_src += conv_type->single.offset.src;
        offset_dst += conv_type->single.offset.dst;
      }
    }
}

dart_ret_t local_get(dart_gptr_t* gptr, gaspi_segment_id_t gaspi_src_segment_id, void* dst, converted_type_t* conv_type)
{
    gaspi_pointer_t src_seg_ptr = NULL;
    DART_CHECK_GASPI_ERROR(gaspi_segment_ptr(gaspi_src_segment_id, &src_seg_ptr));

    local_copy_impl((char *) src_seg_ptr + gptr->addr_or_offs.offset, (char*) dst, conv_type);

    return DART_OK;
}

dart_ret_t local_put(dart_gptr_t* gptr, gaspi_segment_id_t gaspi_dst_segment_id, const void* src, converted_type_t* conv_type)
{
    gaspi_pointer_t dst_seg_ptr = NULL;
    DART_CHECK_GASPI_ERROR(gaspi_segment_ptr(gaspi_dst_segment_id, &dst_seg_ptr));

    local_copy_impl((char *) src, (char*) dst_seg_ptr + gptr->addr_or_offs.offset, conv_type);

    return DART_OK;
}

gaspi_return_t remote_get(dart_gptr_t* gptr, gaspi_rank_t src_unit, gaspi_segment_id_t src_seg_id, gaspi_segment_id_t dst_seg_id, void* dst, gaspi_queue_id_t* queue, converted_type_t* conv_type)
{
    size_t nbytes_segment = 0;
    size_t nbytes_read = 0;

    DART_CHECK_ERROR( dart_get_minimal_queue(queue));

    if(conv_type->kind == DART_BLOCK_SINGLE)
    {
      nbytes_segment = conv_type->num_blocks * conv_type->single.offset.dst + conv_type->single.nbyte;
      nbytes_read = conv_type->single.nbyte;
    }
    else
    {
      size_t last_block = conv_type->num_blocks - 1;
      nbytes_segment = conv_type->multiple.offsets[last_block].dst + conv_type->multiple.nbytes[last_block];
    }

    DART_CHECK_GASPI_ERROR(gaspi_segment_bind(dst_seg_id, dst, nbytes_segment, 0));

    size_t offset_src = gptr->addr_or_offs.offset;
    size_t offset_dst = 0;

    for(int i = 0; i < conv_type->num_blocks; ++i)
    {
      if(conv_type->kind == DART_BLOCK_MULTIPLE)
      {
        offset_src = gptr->addr_or_offs.offset + conv_type->multiple.offsets[i].src;
        offset_dst = conv_type->multiple.offsets[i].dst;
        nbytes_read = conv_type->multiple.nbytes[i];
      }

      DART_CHECK_GASPI_ERROR(
            gaspi_read(dst_seg_id,
                        offset_dst,
                        src_unit,
                        src_seg_id,
                        offset_src,
                        nbytes_read,
                        *queue,
                        GASPI_BLOCK)
     );
      if(conv_type->kind == DART_BLOCK_SINGLE)
      {
        offset_src += conv_type->single.offset.src;
        offset_dst += conv_type->single.offset.dst;
      }
    }

    free_converted_type(conv_type);

    return GASPI_SUCCESS;
}

gaspi_return_t remote_put(dart_gptr_t* gptr, gaspi_rank_t dst_unit, gaspi_segment_id_t dst_seg_id, gaspi_segment_id_t src_seg_id, void* src, gaspi_queue_id_t* queue, converted_type_t* conv_type)
{
    size_t nbytes_segment = 0;
    size_t nbytes_read = 0;

    DART_CHECK_ERROR( dart_get_minimal_queue(queue));

    if(conv_type->kind == DART_BLOCK_SINGLE)
    {
      nbytes_segment = conv_type->num_blocks * conv_type->single.offset.src + conv_type->single.nbyte;
      nbytes_read = conv_type->single.nbyte;
    }
    else
    {
      size_t last_block = conv_type->num_blocks - 1;
      nbytes_segment = conv_type->multiple.offsets[last_block].src + conv_type->multiple.nbytes[last_block];
    }

    DART_CHECK_GASPI_ERROR(gaspi_segment_bind(src_seg_id, src, nbytes_segment, 0));

    size_t offset_src = 0;
    size_t offset_dst = gptr->addr_or_offs.offset;

    for(int i = 0; i < conv_type->num_blocks; ++i)
    {
      if(conv_type->kind == DART_BLOCK_MULTIPLE)
      {
        offset_src = conv_type->multiple.offsets[i].src;
        offset_dst = gptr->addr_or_offs.offset + conv_type->multiple.offsets[i].dst;
        nbytes_read = conv_type->multiple.nbytes[i];
      }

      DART_CHECK_GASPI_ERROR(
            gaspi_write(src_seg_id,
                        offset_src,
                        dst_unit,
                        dst_seg_id,
                        offset_dst,
                        nbytes_read,
                        *queue,
                        GASPI_BLOCK)
     );
      if(conv_type->kind == DART_BLOCK_SINGLE)
      {
        offset_src += conv_type->single.offset.src;
        offset_dst += conv_type->single.offset.dst;
      }
    }

    free_converted_type(conv_type);

    return GASPI_SUCCESS;
}