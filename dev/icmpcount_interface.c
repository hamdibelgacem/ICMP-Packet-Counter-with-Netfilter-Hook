#include <linux/cdev.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/cdev.h>	

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <linux/ip.h>
#include <linux/inet.h>

#include "icmpcount_dev.h"
#include "icmpcount.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Belgacem Hamdi <belgacem.hamdi@ensi-uma.tn>");
MODULE_DESCRIPTION("ICMP counter module using netfilter hooks");
MODULE_VERSION("0.1");

/* store the major number extracted by dev_t */
int icmpcount_d_interface_major = 0;
int icmpcount_d_interface_minor = 0;

#define DEVICE_NAME "icmpcount_d"
char* icmpcount_d_interface_name = DEVICE_NAME;

static struct nf_hook_ops nfho;     // net filter hook option struct 
struct sk_buff *sock_buff;          // socket buffer used in linux kernel
struct iphdr *ip_header;            // ip header struct
struct ethhdr *mac_header;          // mac header struct

icmpcount_d_interface_dev icmpcount_d_interface;

unsigned int counter = 0;

unsigned int hook_func(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
	sock_buff = skb;
	ip_header = (struct iphdr *)skb_network_header(sock_buff); //grab network header using accessor
	mac_header = (struct ethhdr *)skb_mac_header(sock_buff);

	if(!sock_buff) { return NF_DROP;}
	
	if (ip_header->protocol == IPPROTO_ICMP)
	{
		counter++;
		printk(KERN_INFO "count  = %d \n", counter);
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
	memset(icmpcount_d_interface, 0, sizeof(icmpcount_d_interface_dev));
	atomic_set(&icmpcount_d_interface->available, 1);
	sema_init(&icmpcount_d_interface->sem, 1);

	return result;
}

static void icmpcount_d_interface_dev_del(icmpcount_d_interface_dev * icmpcount_d_interface) {}

static int icmpcount_d_interface_setup_cdev(icmpcount_d_interface_dev * icmpcount_d_interface)
{
	int error = 0;
	dev_t devno = MKDEV(icmpcount_d_interface_major, icmpcount_d_interface_minor);

	cdev_init(&icmpcount_d_interface->cdev, &icmpcount_d_interface_fops);
	icmpcount_d_interface->cdev.owner = THIS_MODULE;
	icmpcount_d_interface->cdev.ops = &icmpcount_d_interface_fops;
	error = cdev_add(&icmpcount_d_interface->cdev, devno, 1);

	return error;
}

static void icmpcount_d_interface_exit(void)
{
	printk(KERN_INFO "Cleaning up icmp-packet-counting module.\n");
	nf_unregister_hook(&nfho);
	dev_t devno = MKDEV(icmpcount_d_interface_major, icmpcount_d_interface_minor);

	cdev_del(&icmpcount_d_interface.cdev);
	unregister_chrdev_region(devno, 1);
	icmpcount_d_interface_dev_del(&icmpcount_d_interface);

	printk(KERN_INFO "icmpcount_d_interface: module unloaded\n");
}

static int icmpcount_d_interface_init(void)
{
	dev_t           devno = 0;
	int             result = 0;

	icmpcount_d_interface_dev_init(&icmpcount_d_interface);


	/* register char device
	 * we will get the major number dynamically.
	 */
	result = alloc_chrdev_region(&devno, icmpcount_d_interface_minor, 1, icmpcount_d_interface_name);
	icmpcount_d_interface_major = MAJOR(devno);
	if (result < 0) {
		printk(KERN_WARNING "icmpcount_d_interface: can't get major number %d\n", icmpcount_d_interface_major);
		icmpcount_d_interface_exit();
		return result;
	}

	result = icmpcount_d_interface_setup_cdev(&icmpcount_d_interface);
	if (result < 0) {
		printk(KERN_WARNING "icmpcount_d_interface: error %d adding icmpcount_d_interface", result);
		icmpcount_d_interface_exit();
		return result;
	}

	printk(KERN_INFO "icmpcount_d_interface: module loaded\n");
	
	nfho.hook = hook_func;				// A pointer to a function that is called as soon as the hook is triggered.
	nfho.hooknum = NF_INET_PRE_ROUTING; // NF_IP_PRE_ROUTING=0(capture ICMP Request.)  NF_IP_POST_ROUTING=4(capture ICMP reply.)
	nfho.pf = PF_INET; 					// IPV4 packets
	nfho.priority = NF_IP_PRI_FIRST; 	// Set to highest priority over all other hook functions
	nf_register_hook(&nfho); 			// Register hook

	printk(KERN_INFO "---------------------------------------\n");
	printk(KERN_INFO "Loading icmp-packet-counting kernel module...\n");
	return 0;
}

int icmpcount_d_interface_open(struct inode *inode, struct file *filp)
{
	icmpcount_d_interface_dev *icmpcount_d_interface;

	icmpcount_d_interface = container_of(inode->i_cdev, icmpcount_d_interface_dev, cdev);
	filp->private_data = icmpcount_d_interface;

	if (!atomic_dec_and_test(&icmpcount_d_interface->available)) {
		atomic_inc(&icmpcount_d_interface->available);
		printk(KERN_ALERT "open icmpcount_d_interface : the device has been opened by some other device, unable to open lock\n");
		return -EBUSY;		// already open
	}

	return 0;
}

int icmpcount_d_interface_release(struct inode *inode, struct file *filp)
{
	icmpcount_d_interface_dev *icmpcount_d_interface = filp->private_data;
	atomic_inc(&icmpcount_d_interface->available);	/* release the device */
	return 0;
}

long icmpcount_d_interface_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
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
