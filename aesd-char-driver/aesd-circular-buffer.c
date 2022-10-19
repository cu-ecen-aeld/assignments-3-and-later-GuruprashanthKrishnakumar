/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes, updated by Guruprashanth Krishnakumar
 * @date 2020-03-01, updated on 10/11/2022
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>

#else
#include <string.h>
#endif
#include <stdio.h>
#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    //Check for invalid inputs
    if(!buffer || !entry_offset_byte_rtn)
    {
        return NULL;
    }
    //incremental total size of all the buffers traversed
    int total_size = 0;
    //iterate a maximum times of num_bufs_iterated = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED
    int num_bufs_iterated;
    //starting index should point to buffer->out_offs
    int buf_idx ;
    for(num_bufs_iterated =0,buf_idx =buffer->out_offs;
            num_bufs_iterated<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
            buf_idx = ((buf_idx + 1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED),num_bufs_iterated++)
            {
                //get the aesd_buffer_entry address, if size of data in that = 0, assume enough data not 
                //present since circular buf initialized with memset.
                struct aesd_buffer_entry *entry = &(buffer->entry[buf_idx]);
                if(entry->size == 0)
                {
                    return NULL;
                }
                //if char offset within current buffer, return current buffer along with the relative 
                //offset within the buffer.
                if((total_size + entry->size -1) >= char_offset)
                {
                    
                    *entry_offset_byte_rtn = (char_offset - total_size);
                    return entry;
                }
                //continue incremening total size otherwise
                total_size += entry->size;
            } 
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
char* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    //check for invalid arguments
    char *ret_val = NULL;
    if(!buffer || !add_entry)
    {
        return ret_val;
    }
    //Condition 1 checked in case the reader did not update queue->full flag after reading from a full queue.
    //Condition 2 checked to avoid incrementing out_offs at the initial state
    if(buffer->in_offs == buffer->out_offs && buffer->full)
    {
        ret_val = buffer->entry[buffer->out_offs].buffptr;
        buffer->out_offs = ((buffer->out_offs + 1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED);
    }
    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;
    buffer->in_offs = ((buffer->in_offs + 1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED);
    //check if queue full, otherwise update the flag to false (for safety).
    if(buffer->in_offs == buffer->out_offs)
    {
        buffer->full = true;
    }
    else
    {
        buffer->full = false;
    }
    return ret_val;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
