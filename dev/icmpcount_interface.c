#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/cdev.h>
#include <linux/fs.h>

#include <linux/semaphore.h>
#include <asm/atomic.h>

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>

#include "icmpcount_dev.h"
#include "icmpcount.h"

#define CONTROL_DEVICE_NAME "icmpcounter_dev"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Belgacem Hamdi <belgacem.hamdi@ensi-uma.tn>");
MODULE_DESCRIPTION("ICMP counter module using netfilter hooks");
MODULE_VERSION("0.1");

/* store the major number extracted by dev_t */
int icmpcount_d_interface_major = 0;
int icmpcount_d_interface_minor = 0;

static struct nf_hook_ops nfho;     // net filter hook option struct

icmpcount_d_interface_dev icmpcount_d_interface;

static unsigned int counter;

unsigned int hook_func(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
	struct iphdr *ip_header = NULL;	// ip header struct.
	
	if(!skb) { return NF_ACCEPT;}
	
	ip_header = (struct iphdr *)skb_network_header(skb); //grab network header using accessor
	
	if(ip_header->protocol == IPPROTO_ICMP)
	{
		counter++;
		printk(KERN_DEBUG "ICMP_COUNTER: ICMP packet hooked");
	}
	
	return NF_ACCEPT;
}

struct file_operations icmpcount_d_interface_fops = {
	.open = icmpcount_d_interface_open,
	.unlocked_ioctl = icmpcount_d_interface_ioctl,
	.release = icmpcount_d_interface_release
};

static int icmpcount_d_interface_dev_init(icmpcount_d_interface_dev * icmpcount_d_interface)
{
	int result = 0;
	if(icmpcount_d_interface != NULL)
	{
		memset(icmpcount_d_interface, 0, sizeof(icmpcount_d_interface_dev));
		atomic_set(&icmpcount_d_interface->available, 1);
		sema_init(&icmpcount_d_interface->sem, 1);
	}
	return result;
}

static void icmpcount_d_interface_dev_del(icmpcount_d_interface_dev * icmpcount_d_interface) {}

static int icmpcount_d_interface_setup_cdev(icmpcount_d_interface_dev * icmpcount_d_interface)
{
	int error = 0;
	dev_t devno = MKDEV(icmpcount_d_interface_major, icmpcount_d_interface_minor);

	if(icmpcount_d_interface != NULL)
	{
		cdev_init(&icmpcount_d_interface->cdev, &icmpcount_d_interface_fops);
		icmpcount_d_interface->cdev.owner = THIS_MODULE;
		icmpcount_d_interface->cdev.ops = &icmpcount_d_interface_fops;
		error = cdev_add(&icmpcount_d_interface->cdev, devno, 1);
	}
	return error;
}

static void icmpcount_d_interface_exit(void)
{
	printk(KERN_INFO "ICMP_COUNTER : Cleaning up icmpcount module.\n");
	//nf_unregister_hook(&nfho);
	dev_t devno = MKDEV(icmpcount_d_interface_major, icmpcount_d_interface_minor);

	cdev_del(&icmpcount_d_interface.cdev);
	unregister_chrdev_region(devno, 1);
	icmpcount_d_interface_dev_del(&icmpcount_d_interface);

	printk(KERN_INFO "ICMP_COUNTER: module unloaded\n");
}

static int icmpcount_d_interface_init(void)
{
	dev_t           devno = 0;
	int             result = 0;

	icmpcount_d_interface_dev_init(&icmpcount_d_interface);

	// register char device, will get the major number dynamically.
	result = alloc_chrdev_region(&devno, icmpcount_d_interface_minor, 1, CONTROL_DEVICE_NAME);
	icmpcount_d_interface_major = MAJOR(devno);
	if (result < 0) {
		printk(KERN_WARNING "ICMP_COUNTER: error %d adding icmpcount_d_interface\n", icmpcount_d_interface_major);
		icmpcount_d_interface_exit();
		return result;
	}

	result = icmpcount_d_interface_setup_cdev(&icmpcount_d_interface);
	printk(KERN_INFO "ICMP_COUNTER: module loaded\n");

	return 0;
}

int icmpcount_d_interface_open(struct inode *inode, struct file *filp)
{
	icmpcount_d_interface_dev *icmpcount_d_interface;

	icmpcount_d_interface = container_of(inode->i_cdev, icmpcount_d_interface_dev, cdev);
	filp->private_data = icmpcount_d_interface;

	if (!atomic_dec_and_test(&icmpcount_d_interface->available)) {
		atomic_inc(&icmpcount_d_interface->available);
		printk(KERN_ERR "ICMP_COUNTER : %s has been opened by some other device\n", CONTROL_DEVICE_NAME);
		return -EBUSY;		// already open
	}

	return 0;
}

int icmpcount_d_interface_release(struct inode *inode, struct file *filp)
{
	icmpcount_d_interface_dev *icmpcount_d_interface = filp->private_data;
	atomic_inc(&icmpcount_d_interface->available);	/* release the device */
	printk(KERN_INFO "ICMP_COUNTER: '%s' is close by user", CONTROL_DEVICE_NAME);
	return 0;
}

long icmpcount_d_interface_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
		case START_COUNT:
			if (nfho.hook)
			{
				printk (KERN_ERR "ICMP_COUNTER: ICMP hook already running");
				return -EBUSY;
			}

			printk(KERN_INFO "ICMP_COUNTER: Start counting");

			/** Reset counter & init hook on start */
			counter = 0;
			
			nfho.hook = hook_func;				// A pointer to a function that is called as soon as the hook is triggered.
			nfho.hooknum = NF_INET_PRE_ROUTING; // NF_IP_PRE_ROUTING=0(capture ICMP Request.)  NF_IP_POST_ROUTING=4(capture ICMP reply.)
			nfho.pf = PF_INET; 					// IPV4 packets
			nfho.priority = NF_IP_PRI_FIRST; 	// Set to highest priority over all other hook functions
			
			if (nf_register_net_hook(&init_net, &nfho))
			{
				printk (KERN_ERR "ICMP_COUNTER: Cannot register netfilter");
				return -EBUSY;
			}
			break;

		case STOP_COUNT:
			printk(KERN_INFO "ICMP_COUNTER: Stop counting");
			if (nfho.hook)
			{
				printk(KERN_DEBUG "ICMP_COUNTER: Unregister netfilter hook on stop");
				nf_unregister_net_hook(&init_net, &nfho);
				nfho.hook = NULL;
			}
			break;
		
		case GET_COUNT:
			printk(KERN_INFO "ICMP_COUNTER: Get counter value (%u)", counter);

			if (copy_to_user((char *)arg, &counter, sizeof (int)))
			{
				printk (KERN_ERR "ICMP_COUNTER: Cannot send counter to user");
				return -EBUSY;
			}
			break;
	
		default:
			printk(KERN_ERR "ICMP_COUNTER: Unknown command : %d", cmd);
			break;
	}
	return 0;
}

module_init(icmpcount_d_interface_init);
module_exit(icmpcount_d_interface_exit);
