#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BSU CS 452 HW5");
MODULE_AUTHOR("<patricksantana@u.boisestate.edu>");

typedef struct
{
    dev_t devno;         // Device number
    struct cdev cdev;    // Character device structure
    char *separatorList; // Pointer to a list of separator characters
} Device;

typedef struct
{
    char *s;                    // Pointer to input data
    char *separatorList;        // Pointer to a list of separator characters
    size_t inputSize;           // Size of the input data
    int ioctl;                  // Control flag for device operations
    size_t separatorListLength; // Length of the separator list
    size_t inputProcessedChars; // Number of characters processed from the input data
} Scanner;

static Device device;

/**
 * Called whenever a process attempts to open the device file.
 * It allocates and initializes a Scanner structure and sets up the state for operations on the device.
 *
 * @inode: Pointer to an inode object of the device
 * @filp: File pointer to the device file
 *
 * Return: 0 on success, negative error code on failure.
 */
static int open(struct inode *inode, struct file *filp)
{
    // allocates memory for the scanner instance
    Scanner *scanner = (Scanner *)kmalloc(sizeof(*scanner), GFP_KERNEL);
    if (!scanner)
    {
        printk(KERN_ERR "%s: kmalloc() failed\n", DEVNAME); // log kmalloc failure
        return -ENOMEM;                                     // return error code for memory allocation failure
    }

    // allocates memory for the default list of separators from the Device struct
    scanner->separatorList = kmalloc(strlen(device.separatorList) + 1, GFP_KERNEL);
    if (!scanner->separatorList)
    {
        printk(KERN_ERR "%s: kmalloc() failed for separatorList\n", DEVNAME); // log failure for separatorList allocation
        kfree(scanner);                                                       // free the previously allocated scanner before returning
        return -ENOMEM;                                                       // return error code for memory allocation failure
    }

    // copy the default list of separators from the device struct to the scanner instance
    strcpy(scanner->separatorList, device.separatorList);
    scanner->separatorListLength = strlen(device.separatorList); // stre length of the separator list

    // set ioctl to 1 by default, the next write will not be to separators
    scanner->ioctl = 1;

    // save the scanner instance to the file pointer
    filp->private_data = scanner;

    return 0;
}

/**
 * Called when a process finishes using the device file,
 * It cleans up and frees the resources allocated during the open call
 * like the Scanner structure and its data.
 *
 * @inode: Pointer to an inode object of the device
 * @filp: File pointer to the device file
 *
 * Return: 0 for successful release.
 */
static int release(struct inode *inode, struct file *filp)
{
    // retrieve the scanner instance from the file's private data field
    Scanner *scanner = filp->private_data;

    // free the memory allocated for list of separators
    kfree(scanner->separatorList);

    // free memory allocated for the scanner instance
    kfree(scanner);

    return 0;
}

/**
 * Iterates through the list of separator characters stored
 * in the Scanner to decide if the given character (cmp) is
 * one of the separators. Used to parse and tokenize input based on
 * the separator characters.
 *
 * @scan: Pointer to the Scanner structure
 * @cmp: Character to check against the list of separators
 *
 * Return: 1 if the character is a separator, 0 otherwise.
 */
static int isCharSeparator(Scanner *scan, char cmp)
{
    // iterate through list of separators
    int i;
    for (i = 0; i < scan->separatorListLength; i++)
    {
        // check if current char matches the one being compared
        if (scan->separatorList[i] == cmp)
        {
            return 1; // return 1 if char is found in the separator list
        }
    }

    return 0;
}

/**
 * Reads data from a device file associated with the Scanner device.
 * Reads characters from the Scanner's input and copies them to the buffer.
 * Stops reading when the requested number of characters has been read,
 * a separator character is encountered, or all available input has been processed.
 *
 * @filp:       Pointer to the file structure.
 * @buf:        User buffer to store the read data.
 * @charRequested: Number of characters requested by the user.
 * @f_pos:      Pointer to the current file position.
 *
 * Return: On success, returns the number of characters read. On error, returns an error code.
 */
extern ssize_t read(struct file *filp, char *buf, size_t charRequested, loff_t *f_pos)
{
    // obtain pointer to the scanner structure associated with the file
    Scanner *scan = filp->private_data;

    // init variables to keep track of read
    size_t numCharsProcessed = 0; // number chars read
    int separatorFound = 0;       // flag to indicate if token was found

    // alloc memory for the current token read
    char *currentToken = kmalloc(sizeof(char) * (charRequested + 1), GFP_KERNEL);

    // init allocated memory and check for failure
    memset(currentToken, 0, sizeof(char) * (charRequested + 1));
    if (!currentToken)
    {
        printk(KERN_ERR "%s: kmalloc failed", DEVNAME);
        return -ENOMEM;
    }

    // loop until one of the stopping conditions is met
    while (numCharsProcessed < charRequested && !separatorFound && scan->inputProcessedChars < scan->inputSize)
    {
        // get the current char from the input
        char currChar = scan->s[scan->inputProcessedChars];

        // check if current char is separator
        separatorFound = isCharSeparator(scan, currChar);

        // if not separator, add to token
        if (!separatorFound)
        {
            currentToken[numCharsProcessed] = currChar;
            currentToken[numCharsProcessed + 1] = '\0';

            // inc number of chars in buffer
            numCharsProcessed++;

            // inc position in input stream
            scan->inputProcessedChars++;
        }
    }

    // copy token back to buffer
    if (copy_to_user(buf, currentToken, numCharsProcessed))
    {
        printk(KERN_ERR "%s: copy_to_user() failed\n", DEVNAME);
        return 0;
    }

    // free allocated memory for current token
    kfree(currentToken);

    // set numCharsProcessed for special cases
    if (scan->inputProcessedChars == scan->inputSize && numCharsProcessed == 0)
    {
        numCharsProcessed = -1;
        kfree(scan->s);
    }
    if (separatorFound && numCharsProcessed == 0)
    {
        scan->inputProcessedChars++;
    }

    return numCharsProcessed;
}

/**
 * Handles device-specific control operations
 * for the Scanner device. Receives a control command (cmd) and an argument
 * (arg), and based on their values, it performs specific actions/sets
 * device configuration. Checks if cmd is 0 and arg
 * is 0, and if it is, it sets the ioctl flag to 0 in the Scanner structure.
 *
 * @filp: Pointer to the file structure.
 * @cmd:  Control command to be executed.
 * @arg:  Argument for the control command.
 *
 * Return: 0 to indicate success
 */
static long ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    // get pointer to the scanner structure associated with the file
    Scanner *scan = filp->private_data;

    // check if control cmd is 0 and arg is 0
    if (cmd == 0 && arg == 0)
    {
        // set flag to 0 in Scanner structure
        scan->ioctl = 0;
    }

    return 0;
}

/**
 * Writes data to a device file associated with the Scanner device.
 * Performs one of two actions given the state of the ioctl flag in the
 * Scanner structure. If ioctl is not set, it initializes the separatorList by copying user-provided
 * data (line) and sets ioctl to 1. If ioctl is set, it initializes the input buffer (s) by copying user-provided
 * data (line) and sets the inputSize and inputProcessedChars.
 *
 * @filp:  Pointer to the file structure.
 * @line:  User-provided data to write.
 * @len:   Length of the data to write.
 * @f_pos: Pointer to the current file position.
 *
 * Return:
 * number of bytes written to the device file (len) or -1 for an error.
 */
extern ssize_t write(struct file *filp, const char *line, size_t len, loff_t *f_pos)
{
    // obtain pointer to the Scanner structure fot this file
    Scanner *scan = filp->private_data;

    // check if ioctl is not set
    if (!scan->ioctl)
    {
        // free any existing separatorList
        kfree(scan->separatorList);

        // allocate memory for separatorList and initialize
        scan->separatorList = kmalloc(sizeof(char) * (len + 1), GFP_KERNEL);
        scan->separatorList = memset(scan->separatorList, 0, sizeof(char) * (len + 1));

        // copy user-provided data (line) to separatorList
        if (copy_from_user(scan->separatorList, line, len) != 0)
        {
            printk(KERN_ERR "%s: write separators failed", DEVNAME);
            len = -1;
        }

        // update separatorListLength and set ioctl to 1
        scan->separatorListLength = len;
        scan->ioctl = 1;
    }
    else
    {
        // allocate memory for input buffer (s) and initialize
        scan->s = kmalloc(sizeof(char) * (len + 1), GFP_KERNEL);
        scan->s = memset(scan->s, 0, sizeof(char) * (len + 1));

        // copy user-provided data (line) to the input buffer (s)
        if (copy_from_user(scan->s, line, len) != 0)
        {
            printk(KERN_ERR "%s: write failed", DEVNAME);
            len = -1;
        }

        // update inputSize and reset inputProcessedChars
        scan->inputSize = len;
        scan->inputProcessedChars = 0;
    }

    return len;
}

// Valid ops on device driver
static struct file_operations ops = {
    .open = open,
    .release = release,
    .read = read,
    .write = write,
    .unlocked_ioctl = ioctl,
    .owner = THIS_MODULE};

/**
 * Called when the module is loaded and initializes the Scanner
 * device by initializing the default separator list, allocating a character device region for the device,
 * and initializing and adding the character device to the kernel.
 *
 * Return: 0 on success or error code if failure.
 */
static int __init my_init(void)
{
    // list default separators
    const char *defaultSep = " \t\n:;,+-=!@./#$%&*";
    int err;

    // alloc memory for separatorList and initialize with default separators
    device.separatorList = (char *)kmalloc(strlen(defaultSep) + 1, GFP_KERNEL);
    if (!device.separatorList)
    {
        printk(KERN_ERR "%s: kmalloc failed\n", DEVNAME);
        return -ENOMEM;
    }
    strcpy(device.separatorList, defaultSep);

    // allocate character device region for the device
    err = alloc_chrdev_region(&device.devno, 0, 1, DEVNAME);
    if (err < 0)
    {
        printk(KERN_ERR "%s: alloc_chrdev_region() failed\n", DEVNAME);
        return err;
    }

    // init character device structure and set owner
    cdev_init(&device.cdev, &ops);
    device.cdev.owner = THIS_MODULE;

    // add the character device to kernel
    err = cdev_add(&device.cdev, device.devno, 1);
    if (err)
    {
        printk(KERN_ERR "%s: cdev_add() failed\n", DEVNAME);
        return err;
    }

    // print init message
    printk(KERN_INFO "%s: init\n", DEVNAME);

    return 0;
}

/**
 * Called when the module is unloaded and performs the cleanup
 * tasks for the Scanner device by removing the character device from the kernel,
 * Unregistering the character device region, and freeing the memory allocated for separatorList.
 */
static void __exit my_exit(void)
{
    // remove character device from the kernel
    cdev_del(&device.cdev);

    // unregister character device region
    unregister_chrdev_region(device.devno, 1);

    // free memory allocated for separatorList
    kfree(device.separatorList);

    // print exit message
    printk(KERN_INFO "%s: exit\n", DEVNAME);
}

module_init(my_init);
module_exit(my_exit);