#ifndef __ICMPCOUNT_DEV_DEFINE_H__
#define __ICMPCOUNT_DEV_DEFINE_H__

typedef struct
{
	atomic_t available;
	struct semaphore sem;
	struct cdev cdev;
} icmpcount_d_interface_dev;

int icmpcount_d_interface_open(struct inode *inode, struct file *filp);
int icmpcount_d_interface_release(struct inode *inode, struct file *filp);
long icmpcount_d_interface_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#endif // __ICMPCOUNT_DEV_DEFINE_H__
