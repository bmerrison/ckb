#include "usb.h"
#include "devnode.h"
#include "led.h"
#include "input.h"

int usbhotplug(struct libusb_context* ctx, struct libusb_device* device, libusb_hotplug_event event, void* user_data){
    printf("Got hotplug event\n");
    if(event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED){
        // Device connected: parse device
        return openusb(device);
    } else if(event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT){
        // Device disconnected: look for it in the device list
        for(int i = 1; i < DEV_MAX; i++){
            if(!usbcmp(keyboard[i].dev, device))
                return closeusb(i);
        }
    }
    return 0;
}

void quit(){
    for(int i = 1; i < DEV_MAX; i++){
        // Before closing, set all keyboards back to HID input mode so that the stock driver can still talk to them
        if(keyboard[i].handle){
            setinput(keyboard + i, IN_HID);
            // Stop the uinput device now to ensure no keys get stuck
            inputclose(i);
            // Flush the USB queue and close the device
            while(keyboard[i].queuecount > 0){
                usleep(3333);
                if(usbdequeue(keyboard + i) <= 0)
                    break;
            }
            closeusb(i);
        }
    }
    closeusb(0);
    libusb_exit(0);
}

void sighandler2(int type){
    printf("\nIgnoring signal %d (already shutting down)\n", type);
}

void sighandler(int type){
    signal(SIGTERM, sighandler2);
    signal(SIGINT, sighandler2);
    signal(SIGQUIT, sighandler2);
    printf("\nCaught signal %d\n", type);
    quit();
    exit(0);
}

int main(int argc, char** argv){
    printf("ckb Corsair Keyboard RGB driver v0.1\n");

    // Read parameters
    int fps = 60;
    for(int i = 1; i < argc; i++){
        char* argument = argv[i];
        if(sscanf(argument, "--fps=%d", &fps) == 1){
            if(fps > 60 || fps <= 0){
                // There's no point running higher than 60FPS.
                // The LED controller is locked to 60Hz so it will only cause tearing and/or device freezes.
                printf("Warning: Requested %d FPS but capping at 60\n", fps);
                fps = 60;
            }
        }
    }

#ifdef OS_LINUX
    // Load the uinput module (if it's not loaded already)
    if(system("modprobe uinput") != 0)
        printf("Warning: Failed to load module uinput\n");
#endif

    // Start libusb
    if(libusb_init(0)){
        printf("Fatal: Failed to initialize libusb\n");
        return -1;
    }
    libusb_set_debug(0, LIBUSB_LOG_LEVEL_NONE);
    // Make root keyboard
    umask(0);
    memset(keyboard, 0, sizeof(keyboard));
    keyboard[0].model = -1;
    if(!makedevpath(0))
        printf("Root controller ready at %s0\n", devpath);
    // Enumerate connected devices
    printf("Scanning devices\n");
    libusb_device** devices = 0;
    if(libusb_get_device_list(0, &devices) > 0){
        for(libusb_device** dev = devices; *dev != 0; dev++)
            openusb(*dev);
        libusb_free_device_list(devices, 1);
    } else
        printf("Warning: Failed to scan USB devices\n");
    // Set hotplug callback
    libusb_hotplug_callback_handle hphandle;
    if(libusb_hotplug_register_callback(0, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0, V_CORSAIR, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY, usbhotplug, 0, &hphandle) != LIBUSB_SUCCESS)
        printf("Warning: Failed to activate hot plug callback\n");
    printf("Device scan finished\n");

    // Set up signal handlers for quitting the service.
    signal(SIGTERM, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);

    int frame = 0;
    while(1){
        // No need to run most of these functions on every single frame
        if(!frame){
            // Run hotplug callback
            struct timeval tv = { 0 };
            libusb_handle_events_timeout_completed(0, &tv, 0);
            // Process FIFOs
            for(int i = 0; i < DEV_MAX; i++){
                if(keyboard[i].fifo){
                    const char** lines;
                    int nlines = readlines(keyboard[i].fifo, &lines);
                    for(int j = 0; j < nlines; j++){
                        if(lines[j][0] != 0 && lines[j][1] != 0)
                            readcmd(keyboard + i, lines[j]);
                    }
                }
            }
        }
        // Run the USB queue. Messages must be queued because sending multiple messages at the same time can cause the interface to freeze
        for(int i = 1; i < DEV_MAX; i++){
            if(keyboard[i].handle){
                usbdequeue(keyboard + i);
                // Update indicator LEDs for this keyboard. These are polled rather than processed during events because they don't update
                // immediately and may be changed externally by the OS.
                if(!frame)
                    updateindicators(keyboard + i, 0);
            }
        }
        // Sleep for long enough to achieve the desired frame rate (5 packets per frame).
        usleep(1000000 / fps / 5);
        frame = (frame + 1) % 5;
    }
    quit();
    return 0;
}
