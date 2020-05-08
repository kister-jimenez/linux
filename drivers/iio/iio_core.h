/* The industrial I/O core function defs.
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * These definitions are meant for use only within the IIO core, not individual
 * drivers.
 */

#ifndef _IIO_CORE_H_
#define _IIO_CORE_H_
#include <linux/kernel.h>
#include <linux/device.h>

struct iio_chan_spec;
struct iio_dev;

extern struct device_type iio_device_type;

#define IIO_IOCTL_UNHANDLED	1
struct iio_ioctl_handler {
	struct list_head entry;
	long (*ioctl)(struct iio_dev *indio_dev, struct file *filp,
		      unsigned int cmd, unsigned long arg);
};

long iio_device_ioctl(struct iio_dev *indio_dev, struct file *filp,
		      unsigned int cmd, unsigned long arg);

void iio_device_ioctl_handler_register(struct iio_dev *indio_dev,
				       struct iio_ioctl_handler *h);
void iio_device_ioctl_handler_unregister(struct iio_ioctl_handler *h);

int __iio_add_chan_devattr(const char *postfix,
			   struct iio_chan_spec const *chan,
			   ssize_t (*func)(struct device *dev,
					   struct device_attribute *attr,
					   char *buf),
			   ssize_t (*writefunc)(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t len),
			   u64 mask,
			   enum iio_shared_by shared_by,
			   struct device *dev,
			   struct list_head *attr_list);
void iio_free_chan_devattr_list(struct list_head *attr_list);

ssize_t iio_format_value(char *buf, unsigned int type, int size, int *vals);

/* Event interface flags */
#define IIO_BUSY_BIT_POS 1

#ifdef CONFIG_IIO_BUFFER
struct poll_table_struct;

int iio_device_alloc_chrdev_id(struct device *dev);
void iio_device_free_chrdev_id(struct device *dev);

void iio_device_buffer_attach_chrdev(struct iio_dev *indio_dev);

int iio_device_buffers_init(struct iio_dev *indio_dev);
void iio_device_buffers_cleanup(struct iio_dev *indio_dev);

void iio_device_buffers_put(struct iio_dev *indio_dev);

void iio_disable_all_buffers(struct iio_dev *indio_dev);
void iio_buffer_wakeup_poll(struct iio_dev *indio_dev);

#else

static inline void iio_device_buffer_attach_chrdev(struct iio_dev *indio_dev) {}

static inline int iio_device_buffers_init(struct iio_dev *indio_dev)
{
	return 0;
}

static inline void iio_device_buffers_cleanup(struct iio_dev *indio_dev) {}

static inline void iio_device_buffers_put(struct iio_dev *indio_dev) {}

static inline void iio_disable_all_buffers(struct iio_dev *indio_dev) {}
static inline void iio_buffer_wakeup_poll(struct iio_dev *indio_dev) {}

#endif

int iio_device_register_eventset(struct iio_dev *indio_dev);
void iio_device_unregister_eventset(struct iio_dev *indio_dev);
void iio_device_event_attach_chrdev(struct iio_dev *indio_dev);
void iio_device_wakeup_eventset(struct iio_dev *indio_dev);

struct iio_event_interface;
bool iio_event_enabled(const struct iio_event_interface *ev_int);

#endif
