// fl2000_render.c
//
// (c)Copyright 2017, Fresco Logic, Incorporated.
//
// The contents of this file are property of Fresco Logic, Incorporated and are strictly protected
// by Non Disclosure Agreements. Distribution in any form to unauthorized parties is strictly prohibited.
//
// Purpose: Render Device Support
//

#include "fl2000_include.h"

#define BULK_SIZE		512
#define MAX_TRANSFER		(PAGE_SIZE*16 - BULK_SIZE)
#define GET_URB_TIMEOUT		HZ
#define WRITES_IN_FLIGHT	4

void fl2k_urb_completion(struct urb *urb)
{
	struct urb_node *unode = urb->context;
	struct dev_ctx *fl2k = unode->fl2k;
	unsigned long flags;

	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN)) {
			dev_err(&fl2k->usb_dev->dev, "%s - nonzero write bulk status received: %d\n",
				__func__, urb->status);
			atomic_set(&fl2k->lost_pixels, 1);
		}
	}

	urb->transfer_buffer_length = fl2k->urbs.size; /* reset to actual */

	spin_lock_irqsave(&fl2k->urbs.lock, flags);
	list_add_tail(&unode->entry, &fl2k->urbs.list);
	fl2k->urbs.available++;
	spin_unlock_irqrestore(&fl2k->urbs.lock, flags);

#if 0
	/*
	 * When using fb_defio, we deadlock if up() is called
	 * while another is waiting. So queue to another process.
	 */
	if (fb_defio)
		schedule_delayed_work(&unode->release_urb_work, 0);
	else
#endif
		up(&fl2k->urbs.limit_sem);
}

void fl2k_release_urb_work(struct work_struct *work)
{
	struct urb_node *unode = container_of(work, struct urb_node,
					      release_urb_work.work);

	up(&unode->fl2k->urbs.limit_sem);
}

static void fl2k_free_urb_list(struct dev_ctx *fl2k)
{
	int count = fl2k->urbs.count;
	struct list_head *node;
	struct urb_node *unode;
	struct urb *urb;
	int ret;
	unsigned long flags;

	dev_info(&fl2k->usb_dev->dev, "Waiting for completes and freeing all render urbs\n");

	/* keep waiting and freeing, until we've got 'em all */
	while (count--) {

		/* Getting interrupted means a leak, but ok at shutdown*/
		ret = down_interruptible(&fl2k->urbs.limit_sem);
		if (ret)
			break;

		spin_lock_irqsave(&fl2k->urbs.lock, flags);

		node = fl2k->urbs.list.next; /* have reserved one with sem */
		list_del_init(node);

		spin_unlock_irqrestore(&fl2k->urbs.lock, flags);

		unode = list_entry(node, struct urb_node, entry);
		urb = unode->urb;

		/* Free each separately allocated piece */
		usb_free_coherent(urb->dev, fl2k->urbs.size,
				  urb->transfer_buffer, urb->transfer_dma);
		usb_free_urb(urb);
		kfree(node);
	}
	fl2k->urbs.count = 0;
}


int fl2k_alloc_urb_list(struct dev_ctx *fl2k, int count, size_t size)
{
	int i = 0;
	struct urb *urb;
	struct urb_node *unode;
	char *buf;
	struct usb_host_endpoint *ep =
		usb_pipe_endpoint(fl2k->usb_dev, usb_sndbulkpipe(fl2k->usb_dev, 1));

	spin_lock_init(&fl2k->urbs.lock);

	fl2k->urbs.size = size;
	INIT_LIST_HEAD(&fl2k->urbs.list);

	dev_info(&fl2k->usb_dev->dev, "ep fl2k_alloc_urb_list() pipe %d ep %p",
		 1, ep);

	while (i < count) {
		unode = kzalloc(sizeof(struct urb_node), GFP_KERNEL);
		if (!unode)
			break;
		unode->fl2k = fl2k;

		INIT_DELAYED_WORK(&unode->release_urb_work,
			  fl2k_release_urb_work);

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			kfree(unode);
			break;
		}
		unode->urb = urb;	/* ULLI check udl driver here */

		buf = usb_alloc_coherent(fl2k->usb_dev, MAX_TRANSFER, GFP_KERNEL,
					 &urb->transfer_dma);
		if (!buf) {
			kfree(unode);
			usb_free_urb(urb);
			break;
		}

		/* urb->transfer_buffer_length set to actual before submit */
		usb_fill_bulk_urb(urb, fl2k->usb_dev,
				  usb_sndbulkpipe(fl2k->usb_dev, 1),
				  buf, size,
				  fl2k_urb_completion, unode);
		urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

		list_add_tail(&unode->entry, &fl2k->urbs.list);

		i++;
	}

	sema_init(&fl2k->urbs.limit_sem, i);
	fl2k->urbs.count = i;
	fl2k->urbs.available = i;

	dev_info(&fl2k->usb_dev->dev ,"allocated %d %d byte urbs\n", i, (int) size);

	return i;
}

struct urb *fl2k_get_urb(struct dev_ctx *fl2k)
{
	int ret = 0;
	struct list_head *entry;
	struct urb_node *unode;
	struct urb *urb = NULL;
	unsigned long flags;

	/* Wait for an in-flight buffer to complete and get re-queued */
	ret = down_timeout(&fl2k->urbs.limit_sem, GET_URB_TIMEOUT);
	if (ret) {
		atomic_set(&fl2k->lost_pixels, 1);
		dev_info(&fl2k->usb_dev->dev, "wait for urb interrupted: %x available: %d\n",
		       ret, fl2k->urbs.available);
		goto error;
	}

	spin_lock_irqsave(&fl2k->urbs.lock, flags);

	BUG_ON(list_empty(&fl2k->urbs.list)); /* reserved one with limit_sem */
	entry = fl2k->urbs.list.next;
	list_del_init(entry);
	fl2k->urbs.available--;

	spin_unlock_irqrestore(&fl2k->urbs.lock, flags);

	unode = list_entry(entry, struct urb_node, entry);
	urb = unode->urb;

error:
	return urb;
}

int fl2k_submit_urb(struct dev_ctx *fl2k, struct urb *urb, size_t len)
{
	int ret;

	BUG_ON(len > fl2k->urbs.size);

	urb->transfer_buffer_length = len; /* set to actual payload len */
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret) {
		fl2k_urb_completion(urb); /* because no one else will */
		atomic_set(&fl2k->lost_pixels, 1);
		dev_err(&fl2k->usb_dev->dev, "usb_submit_urb error %d ep %p\n",
			ret, urb->ep);
	}
	return ret;
}


/////////////////////////////////////////////////////////////////////////////////
// P R I V A T E
/////////////////////////////////////////////////////////////////////////////////
//

/*
 * push render_ctx to the bus, with dev_ctx->render.busy_list_lock held
 */
int
fl2000_render_with_busy_list_lock(
	struct dev_ctx * dev_ctx,
	struct render_ctx * render_ctx
	)
{
	struct list_head* const	free_list_head = &dev_ctx->render.free_list;
	int ret_val = 0;
	unsigned long flags;

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_RENDER, ">>>>");

	if (!dev_ctx->monitor_plugged_in) {
		dbg_msg(TRACE_LEVEL_WARNING, DBG_RENDER,
			"WARNING Monitor is not attached.");
		/*
		 * put the render_ctx into free_list_head
		 */
		spin_lock_bh(&dev_ctx->render.free_list_lock);
		list_add_tail(&render_ctx->list_entry, free_list_head);
		spin_unlock_bh(&dev_ctx->render.free_list_lock);

		spin_lock_irqsave(&dev_ctx->count_lock, flags);
		dev_ctx->render.free_list_count++;
		spin_unlock_irqrestore(&dev_ctx->count_lock, flags);
		goto exit;
	}

	/*
	 * put this render_ctx into busy_list
	 */
	list_add_tail(&render_ctx->list_entry, &dev_ctx->render.busy_list);
	spin_lock_irqsave(&dev_ctx->count_lock, flags);
	dev_ctx->render.busy_list_count++;
	spin_unlock_irqrestore(&dev_ctx->count_lock, flags);

	fl2000_bulk_prepare_urb(dev_ctx, render_ctx);
	spin_lock_irqsave(&dev_ctx->count_lock, flags);
	render_ctx->pending_count++;
	spin_unlock_irqrestore(&dev_ctx->count_lock, flags);
	ret_val = usb_submit_urb(render_ctx->main_urb, GFP_ATOMIC);
	if (ret_val != 0) {
		dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
			"[ERR] usb_submit-urb(%p) failed with %d!",
			render_ctx->main_urb,
			ret_val);

		spin_lock_irqsave(&dev_ctx->count_lock, flags);
		render_ctx->pending_count--;
		spin_unlock_irqrestore(&dev_ctx->count_lock, flags);

		/*
		 * remove this render_ctx from busy_list
		 */
		list_del(&render_ctx->list_entry);

		spin_lock_irqsave(&dev_ctx->count_lock, flags);
		dev_ctx->render.busy_list_count--;
		spin_unlock_irqrestore(&dev_ctx->count_lock, flags);

		/*
		 * put the render_ctx into free_list_head
		 */
		spin_lock_bh(&dev_ctx->render.free_list_lock);
		list_add_tail(&render_ctx->list_entry, free_list_head);
		spin_unlock_bh(&dev_ctx->render.free_list_lock);

		spin_lock_irqsave(&dev_ctx->count_lock, flags);
		dev_ctx->render.free_list_count++;
		spin_unlock_irqrestore(&dev_ctx->count_lock, flags);

		if (-ENODEV == ret_val || -ENOENT == ret_val) {
			/*
			 * mark the fl2000 device gone
			 */
			dev_ctx->dev_gone = 1;
		}
	    goto exit;
	}

	if ((dev_ctx->vr_params.end_of_frame_type == EOF_ZERO_LENGTH) &&
	    (VR_TRANSFER_PIPE_BULK == dev_ctx->vr_params.trasfer_pipe)) {
		spin_lock_irqsave(&dev_ctx->count_lock, flags);
		render_ctx->pending_count++;
		spin_unlock_irqrestore(&dev_ctx->count_lock, flags);
		ret_val = usb_submit_urb(
			render_ctx->zero_length_urb, GFP_ATOMIC);
		if (ret_val != 0) {
			dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
				"[ERR] zero_length_urb submit fails with %d.",
				ret_val);

			/*
			 * the main_urb is already schedule, we wait until
			 * the completion to move the render_ctx to free_list
			 */
			spin_lock_irqsave(&dev_ctx->count_lock, flags);
			render_ctx->pending_count--;
			spin_unlock_irqrestore(&dev_ctx->count_lock, flags);
			if (-ENODEV == ret_val || -ENOENT == ret_val) {
				/*
				 * mark the fl2000 device gone
				 */
				dev_ctx->dev_gone = 1;
			}
			goto exit;
		}
	}

exit:
    dbg_msg(TRACE_LEVEL_VERBOSE, DBG_RENDER, "<<<<");
    return ret_val;
}
int fl2k_render_hline(struct dev_ctx *fl2k, const char *front,
		      u32 byte_offset, u32 byte_width)
{
	struct urb *urb;
	char *buf;

	urb = fl2k_get_urb(fl2k);
	if (!urb)
		return -1; /* lost_pixels is set */

	buf = urb->transfer_buffer;

	memcpy(buf, front+byte_offset, byte_width);
	return fl2k_submit_urb(fl2k, urb, byte_width);
}

int fl2k_handle_damage(struct dev_ctx *fl2k,
		       struct render_ctx *node)
{
	int y;
	int ret;
	struct primary_surface *surface = node->primary_surface;
	int width = surface->width;
	int height = surface->height;
	struct urb *urb;

	dev_info(&fl2k->usb_dev->dev, "fl2k handle damage for %d lines", height);
	if (in_irq())
		dev_info(&fl2k->usb_dev->dev, "ERROR fl2k_handle_damage in IRQ");

	for (y = 0; y < height ; y++) {
		const int line_offset = y * width *3;

		ret = fl2k_render_hline(fl2k, surface->render_buffer,
				        line_offset, width * 3);
		if (ret < 0) {
			dev_err(&fl2k->usb_dev->dev, "fl2k fl2k_handle_damage(), no URB");
			return ret;
		}
	}

	urb = fl2k_get_urb(fl2k);
	if (!urb)
		return -1; /* lost_pixels is set */

	fl2k_submit_urb(fl2k, urb, 0);

	return 0;
}

int
fl2000_render_ctx_create(
	struct dev_ctx * dev_ctx
	)
{
	struct render_ctx * render_ctx;
	int		ret_val;
	uint32_t	i;
	unsigned long flags;

	ret_val = 0;
	for (i = 0; i < NUM_OF_RENDER_CTX; i++) {
		render_ctx = &dev_ctx->render.render_ctx[i];

		INIT_LIST_HEAD(&render_ctx->list_entry);
		render_ctx->dev_ctx = dev_ctx;
		render_ctx->pending_count = 0;

		render_ctx->main_urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!render_ctx->main_urb) {
			dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
				"no main_urb usb_alloc_urb?");
			ret_val = -ENOMEM;
			goto exit;
		}

		render_ctx->zero_length_urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!render_ctx->zero_length_urb) {
			dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
				"no zero_length_urb?" );
			ret_val = -ENOMEM;
			goto exit;
		}

		list_add_tail(&render_ctx->list_entry,
			&dev_ctx->render.free_list);

		spin_lock_irqsave(&dev_ctx->count_lock, flags);
		dev_ctx->render.free_list_count++;
		spin_unlock_irqrestore(&dev_ctx->count_lock, flags);
	}

exit:
    return ret_val;
}

void
fl2000_render_ctx_destroy(
    struct dev_ctx * dev_ctx
    )
{
	struct render_ctx * render_ctx;
	uint32_t 	i;
	unsigned long flags;

	for (i = 0; i < NUM_OF_RENDER_CTX; i++) {
		render_ctx = &dev_ctx->render.render_ctx[i];

		// It can be NULL in case of failed initialization.
		//
		if (render_ctx == NULL)
			break;

		if (render_ctx->main_urb) {
		    usb_free_urb( render_ctx->main_urb);
		    render_ctx->main_urb = NULL;
		}

		if (render_ctx->zero_length_urb) {
			usb_free_urb(render_ctx->zero_length_urb);
			render_ctx->zero_length_urb = NULL;
		}

		list_del(&render_ctx->list_entry);

		spin_lock_irqsave(&dev_ctx->count_lock, flags);
		dev_ctx->render.free_list_count--;
		spin_unlock_irqrestore(&dev_ctx->count_lock, flags);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// P U B L I C
/////////////////////////////////////////////////////////////////////////////////
//

int
fl2000_render_create(struct dev_ctx * dev_ctx)
{
	int ret_val;

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_RENDER, ">>>>");

	INIT_LIST_HEAD(&dev_ctx->render.free_list);
	spin_lock_init(&dev_ctx->render.free_list_lock);
	dev_ctx->render.free_list_count = 0;

	INIT_LIST_HEAD(&dev_ctx->render.ready_list);
	spin_lock_init(&dev_ctx->render.ready_list_lock);
	dev_ctx->render.ready_list_count = 0;

	INIT_LIST_HEAD(&dev_ctx->render.busy_list);
	spin_lock_init(&dev_ctx->render.busy_list_lock);
	dev_ctx->render.busy_list_count = 0;

	ret_val = fl2000_render_ctx_create(dev_ctx);
	if (ret_val < 0) {
		dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
			"[ERR] fl2000_render_ctx_create failed?");
		goto exit;
	}

	if (!fl2k_alloc_urb_list(dev_ctx, WRITES_IN_FLIGHT, MAX_TRANSFER)) {
		dev_err(&dev_ctx->usb_dev->dev, "udl_alloc_urb_list failed\n");
		goto exit;
	}

	INIT_LIST_HEAD(&dev_ctx->render.surface_list);
	spin_lock_init(&dev_ctx->render.surface_list_lock);
	dev_ctx->render.surface_list_count = 0;

exit:
	if (ret_val < 0) {
		dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
			"[ERR] Initialize threads failed.");
		fl2000_render_destroy(dev_ctx);
	}

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_RENDER, "<<<<");
	return ret_val;
}

void
fl2000_render_destroy(struct dev_ctx * dev_ctx)
{
	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_RENDER, ">>>>");

	fl2000_render_ctx_destroy(dev_ctx);

	if (dev_ctx->urbs.count)
		fl2k_free_urb_list(dev_ctx);

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_RENDER, "<<<<");
}

void fl2000_render_completion(struct render_ctx * render_ctx)
{
	struct dev_ctx * const dev_ctx = render_ctx->dev_ctx;
	int const urb_status = render_ctx->main_urb->status;
	unsigned long flags;

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_RENDER, ">>>>");

	/*
	 * remove this render_ctx from busy_list
	 */
	spin_lock_bh(&dev_ctx->render.busy_list_lock);
	list_del(&render_ctx->list_entry);
	spin_unlock_bh(&dev_ctx->render.busy_list_lock);

	spin_lock_irqsave(&dev_ctx->count_lock, flags);
	dev_ctx->render.busy_list_count--;
	spin_unlock_irqrestore(&dev_ctx->count_lock, flags);

	/*
	 * put the render_ctx into free_list_head
	 */
	spin_lock_bh(&dev_ctx->render.free_list_lock);
	list_add_tail(&render_ctx->list_entry, &dev_ctx->render.free_list);
	spin_unlock_bh(&dev_ctx->render.free_list_lock);

	spin_lock_irqsave(&dev_ctx->count_lock, flags);
	dev_ctx->render.free_list_count++;
	spin_unlock_irqrestore(&dev_ctx->count_lock, flags);

	if (urb_status < 0) {
		dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
			"urb->status(%d) error", urb_status);
		dev_ctx->render.green_light = 0;
		if (urb_status == -ESHUTDOWN || urb_status == -ENOENT ||
		    urb_status == -ENODEV) {
			dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP, "mark device gone");
			dev_ctx->dev_gone = true;
		    }
		goto exit;
	}
	fl2000_schedule_next_render(dev_ctx);
exit:
	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_RENDER, "<<<<");
}

void fl2000_render_completion_tasklet(unsigned long data)
{
	struct render_ctx * render_ctx = (struct render_ctx *) data;
	fl2000_render_completion(render_ctx);
}

/*
 * schedule a frame buffer for update.
 * the input frame_buffer should be pinned down or resident in kernel sapce
 */
void
fl2000_primary_surface_update(
	struct dev_ctx * 	dev_ctx,
	struct primary_surface* surface)
{
	struct list_head* const	free_list_head = &dev_ctx->render.free_list;
	struct list_head* const	ready_list_head = &dev_ctx->render.ready_list;
	struct render_ctx *	render_ctx;
	uint32_t		retry_count = 0;
	uint32_t		ready_count = 0;
	uint32_t		free_count = 0;
	unsigned long flags;

	might_sleep();

	dev_ctx->render.last_updated_surface = surface;
	dev_ctx->render.last_frame_num = surface->frame_num;

	if (dev_ctx->render.green_light == 0) {
		dbg_msg(TRACE_LEVEL_WARNING, DBG_RENDER, "green_light off");
		goto exit;
	}

retry:
	/*
	 * get render_ctx from free_list_head
	 */
	render_ctx = NULL;
	spin_lock_bh(&dev_ctx->render.free_list_lock);
	if (!list_empty(free_list_head)) {
		render_ctx = list_first_entry(
			free_list_head, struct render_ctx, list_entry);
		list_del(&render_ctx->list_entry);

		spin_lock_irqsave(&dev_ctx->count_lock, flags);
		dev_ctx->render.free_list_count--;
		spin_unlock_irqrestore(&dev_ctx->count_lock, flags);
	}
	spin_unlock_bh(&dev_ctx->render.free_list_lock);

	if (render_ctx == NULL) {
		if (retry_count > 3) {
			dbg_msg(TRACE_LEVEL_WARNING, DBG_RENDER,
				"no render_ctx?");
			return;
		}
		retry_count++;
		msleep_interruptible(10);
		goto retry;
	}

	/*
	 * by now we have a render_ctx, initialize it and schedule it
	 */
	render_ctx->primary_surface = surface;

	spin_lock_bh(&dev_ctx->render.ready_list_lock);
	list_add_tail(&render_ctx->list_entry, ready_list_head);

	spin_lock_irqsave(&dev_ctx->count_lock, flags);
	ready_count = ++dev_ctx->render.ready_list_count;
	spin_unlock_irqrestore(&dev_ctx->count_lock, flags);

	spin_unlock_bh(&dev_ctx->render.ready_list_lock);

	dbg_msg(TRACE_LEVEL_WARNING, DBG_RENDER,
		"render_ctx(%p) scheduled, free_count(%u)/ready_count(%u)",
		render_ctx, free_count, ready_count);

	/*
	 * schedule render context from ready_list
	 */
	fl2000_schedule_next_render(dev_ctx);

exit:
	return;
}

/*
 * schedule all render_ctx on ready_list. We schedule redundant frames if no
 * new frames are available.
 */
void
fl2000_schedule_next_render(struct dev_ctx * dev_ctx)
{
	struct list_head* const	free_list_head = &dev_ctx->render.free_list;
	struct list_head* const	ready_list_head = &dev_ctx->render.ready_list;
	struct list_head	staging_list;
	struct render_ctx *	render_ctx = NULL;
	uint32_t		ready_count = 0;
	unsigned long 		flags;
	int			ret_val;

	if (dev_ctx->render.green_light == 0) {
		dbg_msg(TRACE_LEVEL_WARNING, DBG_RENDER, "green_light off");
		goto exit;
	}

	/*
	 * step 1: take out all render_ctx on ready list, and put them to
	 * staging list
	 */
	INIT_LIST_HEAD(&staging_list);
	spin_lock_bh(&dev_ctx->render.ready_list_lock);
	while (!list_empty(ready_list_head)) {
		render_ctx = list_first_entry(
			ready_list_head, struct render_ctx, list_entry);
		list_del(&render_ctx->list_entry);
		spin_lock_irqsave(&dev_ctx->count_lock, flags);
		ready_count = --dev_ctx->render.ready_list_count;
		spin_unlock_irqrestore(&dev_ctx->count_lock, flags);
		list_add_tail(&render_ctx->list_entry, &staging_list);
	}
	ASSERT(ready_count == 0);
	spin_unlock_bh(&dev_ctx->render.ready_list_lock);

	/*
	 * step 2, schedule all render_ctx, with busy_list_lock held.
	 * this is critical path where we schedule redundant frames.
	 */
	spin_lock_bh(&dev_ctx->render.busy_list_lock);
reschedule:
	while (!list_empty(&staging_list)) {
		render_ctx = list_first_entry(
			&staging_list, struct render_ctx, list_entry);
		list_del(&render_ctx->list_entry);
		ret_val = fl2k_handle_damage(dev_ctx, render_ctx);
		if (ret_val < 0) {
			dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
				"usb_submit_urb failed %d, "
				"turn off green_light\n", ret_val);
			dev_ctx->render.green_light = false;
			break;
		}
	}

	/*
	 * schedule redundant frames.
	 */
	if (dev_ctx->render.busy_list_count < NUM_RENDER_ON_BUS &&
	    dev_ctx->render.green_light) {
		struct primary_surface* surface;

		spin_lock_bh(&dev_ctx->render.free_list_lock);
		if (!list_empty(free_list_head)) {
			render_ctx = list_first_entry(
				free_list_head, struct render_ctx, list_entry);
			list_del(&render_ctx->list_entry);

			spin_lock_irqsave(&dev_ctx->count_lock, flags);
			dev_ctx->render.free_list_count--;
			spin_unlock_irqrestore(&dev_ctx->count_lock, flags);
		}
		spin_unlock_bh(&dev_ctx->render.free_list_lock);

		/*
		 * preparing additional frame
		 */
		if (render_ctx != NULL) {
			surface = dev_ctx->render.last_updated_surface;
			render_ctx->primary_surface = surface;
			list_add_tail(&render_ctx->list_entry, &staging_list);
			goto reschedule;
		}
	}
	spin_unlock_bh(&dev_ctx->render.busy_list_lock);

exit:
	return;
}

void fl2000_render_start(struct dev_ctx * dev_ctx)
{
	dev_ctx->render.green_light = 1;
}

void fl2000_render_stop(struct dev_ctx * dev_ctx)
{
	uint32_t delay_ms = 0;

	might_sleep();
	dev_ctx->render.green_light = 0;

	dbg_msg(TRACE_LEVEL_INFO, DBG_PNP,
		"busy_list_count(%u)", dev_ctx->render.busy_list_count);

	while (dev_ctx->render.busy_list_count != 0) {
		delay_ms += 10;
		DELAY_MS(10);
	}
	dbg_msg(TRACE_LEVEL_INFO, DBG_PNP,
		"waited %u ms", delay_ms);

}

// eof: fl2000_render.c
//
