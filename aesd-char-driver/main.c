/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h> //size_t    
#include <linux/cdev.h>
#include <linux/kernel.h> //containerof
#include <linux/slab.h> //kmalloc
#include <linux/uaccess.h>	/* copy_*_user */
#include <linux/fs.h> // file_operations

#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Guruprashanth Krishnakumar"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static int allocate_memory(struct aesd_buffer_entry *ptr, int size);

static int allocate_memory(struct aesd_buffer_entry *ptr, int size)
{
    int retval = 0;
    if(ptr->size == 0)
    {
        ptr->buffptr = kmalloc(size,GFP_KERNEL);
        if(!ptr->buffptr)
        {
            retval = -ENOMEM;
            //Add a goto statement. We need to free COPY_BUFFER and 
            //UNLOCK MUTEX   
        }
    }
    else
    {
        struct aesd_buffer_entry *temp_ptr = krealloc(ptr->buffptr,size,GFP_KERNEL);
        if(!temp_ptr)
        {
            free(ptr->buffptr);
            retval = -ENOMEM;
            //Add a goto statement. We need to free COPY_BUFFER and WORKING_BUFFER
            //UNLOCK MUTEX   
            
        }
        else
        {
            ptr->buffptr = temp_ptr;
        }
    }
    return retval;
}

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *info_dev;
    info_dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = info_dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *data = filp->private_data;
    if (mutex_lock_interruptible(&data->lock))
    {
        retval = -EINTR;
        
        //Add a goto statement to the end
        goto ret_func;
    }
    size_t relative_byte_offset;
    struct aesd_buffer_entry *ret_buf;
    aesd_buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(data,
                                                                        *f_pos, 
                                                                         &relative_byte_offset);
    if(!aesd_buffer_entry)
    {
        //free MUTEX and exit with retval as 0 (EOF reached)
        goto free_lock;
    }
    int bytes_to_eob = (aesd_buffer_entry->size - relative_byte_offset);
    int bytes_to_copy = bytes_to_eob<count?bytes_to_eob:count;
    if (copy_to_user(buf, (ret_buf->buffptr + relative_byte_offset), bytes_to_copy)) 
    {
		retval = -EFAULT;
		//free MUTEX and exit with retval as -EFAULT
        goto free_lock;
	}
    //return number of bytes copied to buffer
    ret_val += bytes_to_copy;
    free_lock: mutex_unlock(&data->lock);
    ret_func: return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    if(count == 0)
    {
        goto ret_func;
    }

    struct aesd_dev *data = filp->private_data;
    if (mutex_lock_interruptible(&data->lock))
    {
        retval = -EINTR;
        
        //Add a goto statement to the end
        goto ret_func;
    }
    struct aesd_buffer_entry copy_buffer;
    //Allocate normal kernel ram. May sleep.
    copy_buffer.buffptr = kmalloc(count,GFP_KERNEL);
    if(!copy_buffer.buffptr)
    {
        retval = -ENOMEM;
        
        //Add a goto statement, need to unlock mutex
        goto release_lock;
    }
    copy_buffer.size = count;
    
    //Copy the data from user space buffer into kernel space 
    if (copy_from_user(copy_buffer.buffptr, buf, count)) 
    {
		retval = -EFAULT;
        
		//Add a goto statement. We need to free COPY_BUFFER and 
        //UNLOCK MUTEX   
        goto release_lock;
	}
    //WRITE LOGIC
    //Parse the copy buffer 
    int start_ptr = 0;
    int mem_to_malloc;
    for(int i = 0;i<count;i++)
    {
        if(copy_buffer[start_ptr+i]=='\n')
        {
            //check if working buffer size is 0, if it is malloc
            //else realloc 
            mem_to_malloc = (i-start_ptr) +1;

            if(allocate_memory(data->working_buffer.buffptr, mem_to_malloc)<0)
            {
                retval = -ENOMEM;
		        //Add a goto statement. We need to free COPY_BUFFER and 
                //UNLOCK MUTEX   
                goto copy_buff_free;
            }
            //CHECK if RIGHT
            memcpy((data->working_buffer.buffptr + data->working_buffer.size),(copy_buffer+start_ptr),mem_to_malloc);
            char *free_buffer = aesd_circular_buffer_add_entry(data,&data->working_buffer);
            if(free_buffer)
            {
                kfree(free_buffer);
            }
            data->working_buffer.buffptr = NULL;
            data->working_buffer.size = 0;
            start_ptr = i+1;
            ret_val += mem_to_malloc;
        }
        if(i == count - 1)
        {
            //incomplete packet transmitted
            if(copy_buffer[i] != '\n')
            {
                mem_to_malloc = (i-start_ptr) +1;
                //check if working buffer size is 0, if it is malloc
                //else realloc 
                if(allocate_memory(data->working_buffer.buffptr, mem_to_malloc)<0)
                {
                    retval = -ENOMEM;
                    
		            //Add a goto statement. We need to free COPY_BUFFER and 
                    //UNLOCK MUTEX   
                    goto copy_buff_free;
                }
                memcpy((data->working_buffer.buffptr + data->working_buffer.size),(copy_buffer+start_ptr),mem_to_malloc);
                data->working_buffer.size = mem_to_malloc;
                ret_val += mem_to_malloc;
            }
        }
    }
    copy_buff_free: free(copy_buffer.buffptr);
    release_lock: mutex_unlock(&data->lock);
    ret_func: return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));
    mutex_init(&aesd_device.lock);
    /**
     * TODO: initialize the AESD specific portion of the device
     */

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    mutex_destroy(&aesd_device.lock);
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
