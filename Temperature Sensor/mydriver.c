#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#define DRIVER_NAME "bmp280"
#define DRIVER_CLASS "bmp280Class"

/* BMP280 register addresses */
#define BMP280_REG_ID 0xD0
#define BMP280_REG_RESET 0xE0
#define BMP280_REG_STATUS 0xF3
#define BMP280_REG_CTRL_MEAS 0xF4
#define BMP280_REG_CONFIG 0xF5
#define BMP280_REG_TEMP_MSB 0xFA
#define BMP280_REG_TEMP_LSB 0xFB
#define BMP280_REG_TEMP_XLSB 0xFC

/* BMP280 calibration registers */
#define BMP280_REG_DIG_T1 0x88
#define BMP280_REG_DIG_T2 0x8A
#define BMP280_REG_DIG_T3 0x8C

#define BMP280_CHIP_ID 0x58     /* BMP280 chip id */
#define BMP280_RESET_VALUE 0xB6 /* Reset value */

#define I2C_BUS_AVAILABLE 1 /* The I2C Bus available on the raspberry */
#define SLAVE_DEVICE_NAME "BMP280"
#define BMP280_SLAVE_ADDRESS 0x76 /* Can be 0x77 if SDO is connected to VDDIO */

static struct i2c_adapter *bmp_i2c_adapter = NULL;
static struct i2c_client *bmp280_i2c_client = NULL;

/* Variables for Device and Deviceclass */
static dev_t myDeviceNr;
static struct class *myClass;
static struct cdev myDevice;

/* Calibration data */
struct bmp280_calibration_data
{
    u16 dig_T1;
    s16 dig_T2;
    s16 dig_T3;
};

static struct bmp280_calibration_data cal_data;

/* Read calibration data from BMP280 */
static int bmp280_read_calibration_data(struct i2c_client *client)
{
    int ret;

    ret = i2c_smbus_read_word_data(client, BMP280_REG_DIG_T1);
    if (ret < 0)
        return ret;
    cal_data.dig_T1 = ret;

    ret = i2c_smbus_read_word_data(client, BMP280_REG_DIG_T2);
    if (ret < 0)
        return ret;
    cal_data.dig_T2 = (s16)ret;

    ret = i2c_smbus_read_word_data(client, BMP280_REG_DIG_T3);
    if (ret < 0)
        return ret;
    cal_data.dig_T3 = (s16)ret;

    return 0;
}

/* Initialize BMP280 sensor */
static int bmp280_init_sensor(struct i2c_client *client)
{
    int ret;
    u8 chip_id;

    /* Read and verify chip ID */
    ret = i2c_smbus_read_byte_data(client, BMP280_REG_ID);
    if (ret < 0)
    {
        printk(KERN_ERR "Failed to read chip ID\n");
        return ret;
    }
    chip_id = (u8)ret;
    if (chip_id != BMP280_CHIP_ID)
    {
        printk(KERN_ERR "Wrong chip ID: 0x%x\n", chip_id);
        return -ENODEV;
    }

    /* Reset the sensor */
    ret = i2c_smbus_write_byte_data(client, BMP280_REG_RESET, BMP280_RESET_VALUE);
    if (ret < 0)
        return ret;
    msleep(10); /* Wait for reset to complete */

    /* Read calibration data */
    ret = bmp280_read_calibration_data(client);
    if (ret < 0)
        return ret;

    /* Configure sensor */
    ret = i2c_smbus_write_byte_data(client, BMP280_REG_CONFIG, (0x04 << 2));
    if (ret < 0)
        return ret;

    ret = i2c_smbus_write_byte_data(client, BMP280_REG_CTRL_MEAS, (0x05 << 5) | (0x00 << 2) | 0x03);
    if (ret < 0)
        return ret;

    return 0;
}

/* Read and calculate temperature */
static s32 read_temperature(void)
{
    int ret;
    u32 raw_temp;
    s32 var1, var2;
    u8 data[3];

    /* Read raw temperature */
    ret = i2c_smbus_read_byte_data(bmp280_i2c_client, BMP280_REG_TEMP_MSB);
    if (ret < 0)
        return ret;
    data[0] = (u8)ret;

    ret = i2c_smbus_read_byte_data(bmp280_i2c_client, BMP280_REG_TEMP_LSB);
    if (ret < 0)
        return ret;
    data[1] = (u8)ret;

    ret = i2c_smbus_read_byte_data(bmp280_i2c_client, BMP280_REG_TEMP_XLSB);
    if (ret < 0)
        return ret;
    data[2] = (u8)ret;

    raw_temp = ((u32)data[0] << 12) | ((u32)data[1] << 4) | ((u32)data[2] >> 4);

    /* Temperature compensation formula from datasheet */
    var1 = ((((raw_temp >> 3) - ((s32)cal_data.dig_T1 << 1))) *
            ((s32)cal_data.dig_T2)) >>
           11;

    var2 = (((((raw_temp >> 4) - ((s32)cal_data.dig_T1)) *
              ((raw_temp >> 4) - ((s32)cal_data.dig_T1))) >>
             12) *
            ((s32)cal_data.dig_T3)) >>
           14;

    return ((var1 + var2) * 5 + 128) >> 8;
}

/* File operations */
static int driver_open(struct inode *deviceFile, struct file *instance)
{
    printk("BMP280 driver - Open was called\n");
    return 0;
}

static int driver_close(struct inode *deviceFile, struct file *instance)
{
    printk("BMP280 driver - Close was called\n");
    return 0;
}

static ssize_t driver_read(struct file *File, char *user_buffer, size_t count, loff_t *offs)
{
    int to_copy, not_copied, delta;
    char out_string[20];
    int temperature;

    temperature = read_temperature();
    if (temperature < 0)
    {
        printk(KERN_ERR "Error reading temperature: %d\n", temperature);
        return temperature;
    }

    to_copy = min(sizeof(out_string), count);
    snprintf(out_string, sizeof(out_string), "%d.%d\n", temperature / 100, temperature % 100);
    not_copied = copy_to_user(user_buffer, out_string, to_copy);
    delta = to_copy - not_copied;

    return delta;
}

/* File operations structure */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .read = driver_read};

/* I2C device ID structure */
static const struct i2c_device_id bmp280_id[] = {
    {SLAVE_DEVICE_NAME, 0},
    {}};
MODULE_DEVICE_TABLE(i2c, bmp280_id);

/* I2C probe function */
static int bmp280_probe(struct i2c_client *client)
{
    int ret;
    printk("BMP280 probe called\n");

    bmp280_i2c_client = client;

    /* Initialize the BMP280 device */
    ret = bmp280_init_sensor(client);
    if (ret < 0)
    {
        printk(KERN_ERR "Failed to initialize BMP280 sensor\n");
        return ret;
    }

    return 0;
}

/* I2C remove function */
static void bmp280_remove(struct i2c_client *client)
{
    printk("BMP280 remove called\n");
}

/* I2C driver structure */
static struct i2c_driver bmp280_driver = {
    .driver = {
        .name = SLAVE_DEVICE_NAME,
        .owner = THIS_MODULE},
    .probe = bmp280_probe,
    .remove = bmp280_remove,
    .id_table = bmp280_id};

/* Module initialization */
static int __init ModuleInit(void)
{
    printk("BMP280 driver - Hello Kernel\n");

    /* Allocate device number */
    if (alloc_chrdev_region(&myDeviceNr, 0, 1, DRIVER_NAME) < 0)
    {
        printk("Device number could not be allocated!\n");
        return -1;
    }

    /* Create device class */
    if ((myClass = class_create(DRIVER_CLASS)) == NULL)
    {
        printk("Device class cannot be created!\n");
        goto ClassError;
    }

    /* Create device file */
    if (device_create(myClass, NULL, myDeviceNr, NULL, DRIVER_NAME) == NULL)
    {
        printk("Cannot create device file!\n");
        goto FileError;
    }

    /* Initialize device file */
    cdev_init(&myDevice, &fops);

    /* Add device to kernel */
    if (cdev_add(&myDevice, myDeviceNr, 1) == -1)
    {
        printk("Registering of device to kernel failed!\n");
        goto AddError;
    }

    /* Register I2C driver */
    bmp_i2c_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);
    if (bmp_i2c_adapter == NULL)
    {
        printk("I2C adapter not found!\n");
        goto AddError;
    }

    /* Create I2C client */
    bmp280_i2c_client = i2c_new_client_device(bmp_i2c_adapter,
                                              &(struct i2c_board_info){
                                                  .type = SLAVE_DEVICE_NAME,
                                                  .addr = BMP280_SLAVE_ADDRESS});
    if (IS_ERR(bmp280_i2c_client))
    {
        printk("Failed to create I2C client\n");
        goto I2CError;
    }

    if (i2c_add_driver(&bmp280_driver) != 0)
    {
        printk("I2C driver registration failed!\n");
        goto I2CError;
    }

    i2c_put_adapter(bmp_i2c_adapter);
    return 0;

I2CError:
    i2c_put_adapter(bmp_i2c_adapter);
AddError:
    device_destroy(myClass, myDeviceNr);
FileError:
    class_destroy(myClass);
ClassError:
    unregister_chrdev_region(myDeviceNr, 1);
    return -1;
}

static void __exit ModuleExit(void)
{
    printk("BMP280 driver - Goodbye Kernel!\n");
    i2c_del_driver(&bmp280_driver);
    i2c_unregister_device(bmp280_i2c_client);
    cdev_del(&myDevice);
    device_destroy(myClass, myDeviceNr);
    class_destroy(myClass);
    unregister_chrdev_region(myDeviceNr, 1);
}

MODULE_AUTHOR("Shabari");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A driver for reading out a BMP280 temperature sensor");

module_init(ModuleInit);
module_exit(ModuleExit);