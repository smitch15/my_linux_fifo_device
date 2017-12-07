
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");

int times = 0;
int write_count = 0;
int buffSize = 0;

int copyRet;
static struct miscdevice my_device;
static int buffer_size;
char* device_name = "practFifo name";
static int buffer_empty_slots;
static int writeI, readI=0;
static DEFINE_SEMAPHORE(full);
static DEFINE_SEMAPHORE(empty);
static DEFINE_MUTEX(mut);

module_param(buffer_size, int, 0);
int* buffer;

static int my_open(struct inode*, struct file*);
static int my_release(struct inode*, struct file*);
static ssize_t my_read(struct file* _file, int* num_read, size_t intSize, loff_t* offset);
static ssize_t my_write(struct file* _file, const int* num_write, size_t intSize, loff_t* offset);

/*fops struct*/
static struct file_operations my_device_fops = {
	.open = &my_open,
	.read = &my_read,
	.write = &my_write,
	.release = &my_release
};

/*initialize module, allocate memory, initialize semaphores ...*/
int init_module(){

	/////////////////////// initialize and register device ///////
	/*initializing parameters for my_device*/
	my_device.name = device_name;
	my_device.minor = MISC_DYNAMIC_MINOR;
	my_device.fops = &my_device_fops;

	sema_init(&full, 0);//might need to be 1, not 0
	sema_init(&empty, buffer_size);
	mutex_init(&mut);

	int register_return_value;
	/*registering the device*/
	register_return_value = misc_register(&my_device);
	if (register_return_value != 0){
		/*misc_register() returns
0: success
-ve: failure*/
		printk(KERN_ERR "Could not register the device\n");
		return register_return_value;
	}
	printk(KERN_INFO "Device Registered!\n");
	////////////////////////////////////////////////////////////////////////////////

	buffer = (int*)kzalloc(buffer_size*sizeof(int), GFP_KERNEL);
	buffer_empty_slots = buffer_size;

	printk(KERN_INFO "buffer size: %d\n", buffer_size);
	printk(KERN_INFO "buffer empty slots: %d\n", buffer_empty_slots);

	printk(KERN_INFO "successfully finished initialization\n");
	return 0;
}

/*function is called when device is opened*/
static int my_open(struct inode* _inode, struct file* _file){
	/*incrementing the number of open devices*/
	printk(KERN_INFO "successfully finished open\n");
	return 0;
}

/*function is called when read() is called on the device*/
static ssize_t my_read(struct file* _file, int* num_read, size_t intSize, loff_t* offset){
	int bytesRead; 
	bytesRead = 0;
	printk(KERN_INFO "successfully entered read\n");
	if (down_interruptible(&full)){
		printk(KERN_INFO "FAILED SEM DOWN CALL\n");
		return -EINTR;
	}
	if (mutex_lock_interruptible(&mut)){
		printk(KERN_INFO "FAILED MUTEX LOCK CALL\n");
		return -EINTR;
	}
	readI %= buffer_size;
	printk(KERN_INFO "read: read index: %d\n", readI);
	printk(KERN_INFO "read: buffer empty slots: %d\n", buffer_empty_slots);
	if (buffer_empty_slots <= buffer_size && buffer_empty_slots >= 0){
		copyRet = copy_to_user(num_read, &buffer[readI], intSize);
		printk(KERN_INFO "successfully executed read\n");
		if (copyRet != 0){
			printk(KERN_INFO "copy to user fail num: %d\n", copyRet);
			return -EFAULT;
		}else{
			bytesRead = 4;
			++readI;
			++buffer_empty_slots;
		}
	}
	mutex_unlock(&mut);
	up(&empty);
	return bytesRead;
}

/*function called when device is written to*/
static ssize_t my_write(struct file* _file, const int* num_write, size_t intSize, loff_t* offset){
	printk(KERN_INFO "successfully entered write\n");
	int copyWret;
	if (down_interruptible(&empty)){
		printk(KERN_INFO "FAILED SEM DOWN CALL\n");
		return -EINTR;
	}
	if (mutex_lock_interruptible(&mut)){
		printk(KERN_INFO "FAILED MUTEX LOCK CALL\n");
		return -EINTR;
	}
	writeI %= buffer_size;
	printk(KERN_INFO "write: write index: %d\n", writeI);
	printk(KERN_INFO "write buffer empty slots: %d\n", buffer_empty_slots);
	if(buffer_empty_slots > 0){
		copyWret = copy_from_user(&buffer[writeI], num_write, intSize);
		printk(KERN_INFO "successfully executed write\n");
		if (copyWret != 0){
			printk(KERN_INFO "copy failed, ret value: %d\n", copyWret);
			return -EFAULT;
		}else{
			++writeI;
			--buffer_empty_slots;
		}
	}
	mutex_unlock(&mut);

	up(&full);
	printk(KERN_INFO "successfully exited write\n");
	return intSize;
}

/*function that is called when device is closed*/
static int my_release(struct inode* _inode, struct file* _file){
	printk(KERN_INFO "successfully entered release\n");
	printk(KERN_INFO "successfully exited release\n");
	return 0;
}

/*function to cleanup the module*/
void cleanup_module(){
	/*freeing memory*/
	kfree(buffer);
	misc_deregister(&my_device);
	printk(KERN_INFO "Device %s Unregistered!\n", device_name);
}

