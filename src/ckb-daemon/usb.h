#ifndef USB_H
#define USB_H

#include "includes.h"
#include "keyboard.h"

// Vendor/product codes
#define V_CORSAIR   0x1b1c
#define P_K70       0x1b13
#define P_K95       0x1b11

// Key binding structures

// Action triggered when activating a macro
typedef struct {
    short scan;
    // down = 0 for keyup, down = 1 for keydown
    char down;
} macroaction;

// Key macro
typedef struct {
    macroaction* actions;
    int actioncount;
    unsigned char combo[N_KEYS / 8];
    char triggered;
} keymacro;

// Key bindings for a device/profile
typedef struct {
    // Base bindings
    int base[N_KEYS];
    // Macros
    keymacro* macros;
    int macrocount;
    int macrocap;
} keybind;
#define MACRO_MAX   1024

// End key bind structures

// Lighting structure for a device/profile
typedef struct {
    char r[N_KEYS / 2];
    char g[N_KEYS / 2];
    char b[N_KEYS / 2];
    char enabled;
} keylight;

// ID structure
typedef struct {
    char guid[16];
    char modified[2];
} usbid;

// Mode structure
#define MD_NAME_LEN 16
typedef struct {
    keylight light;
    keybind bind;
    unsigned short name[MD_NAME_LEN];
    usbid id;
} usbmode;

// Profile structure
#define PR_NAME_LEN 16
typedef struct {
    usbmode* mode;
    int modecount;
    int modecap;
    usbmode* currentmode;
    unsigned short name[PR_NAME_LEN];
    usbid id;
} usbprofile;
#define MODE_MAX    16

// Structure to store settings for a USB device, whether or not it's plugged in
#define SERIAL_LEN  33
typedef struct {
    usbprofile profile;
    char serial[SERIAL_LEN];
} usbsetting;

// Structure for tracking keyboard devices
#define NAME_LEN    33
#define QUEUE_LEN   40
#define MSG_SIZE    64
typedef struct {
    // USB device info
    struct libusb_device_descriptor descriptor;
    libusb_device* dev;
    libusb_device_handle* handle;
    int model;
    // Interrupt transfers
    struct libusb_transfer* keyint;
    unsigned char intinput[MSG_SIZE];
    unsigned char previntinput[N_KEYS / 8];
    // Indicator LED state
    unsigned char ileds;
    // Command FIFO
    int fifo;
    // uinput/event devices
#ifdef OS_LINUX
    int uinput;
    int event;
#endif
#ifdef OS_MAC
    CGEventSourceRef event;
    CGEventFlags eflags;
#endif
    // USB output queue
    unsigned char* queue[QUEUE_LEN];
    int queuecount;
    // Keyboard settings
    usbsetting setting;
    // Device name
    char name[NAME_LEN];
} usbdevice;
#define DEV_MAX     10
extern usbdevice keyboard[DEV_MAX];

// USB device compare. Returns 0 if devices are the same
int usbcmp(libusb_device* dev1, libusb_device* dev2);
// Open a USB device and create a new device entry. Returns 0 on success
int openusb(libusb_device* device);
// Close a USB device and remove device entry. Returns 0 on success
int closeusb(int index);

// Set input mode on a device
#define IN_CORSAIR  0x40
#define IN_HID      0x80
void setinput(usbdevice* kb, int input);

// Add a message to a USB device to be sent to the device. Returns 0 on success.
int usbqueue(usbdevice* kb, unsigned char* messages, int count);
// Output a message from the USB queue to the device, if any. Returns number of bytes written.
int usbdequeue(usbdevice* kb);

// Find a connected USB device. Returns 0 if not found
usbdevice* findusb(const char* serial);
// Find a USB device from storage. Returns 0 if not found
usbsetting* findstore(const char* serial);
// Add a USB device to storage. Returns an existing device if found or a new one if not.
usbsetting* addstore(const char* serial);

// Get a mode from a profile. The mode will be created if it didn't already exist.
usbmode* getusbmode(int id, usbprofile* profile);

// Sets a mode's name
void setmodename(usbmode* mode, const char* name);
// Sets a profile's name
void setprofilename(usbprofile* profile, const char* name);
// Resets a mode to its default settings
void erasemode(usbmode* mode);
// Erases a profile, deleting all of its modes.
void eraseprofile(usbprofile* profile);

// Generates a new ID
void genid(usbid* id);
// Updates an ID's modification
void updatemod(usbid* id);

// Loads the profile name from hardware
void hwloadprofile(usbdevice* kb);
// Saves the profile name to hardware
void hwsaveprofile(usbdevice* kb);

#endif
