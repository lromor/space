# Space

A small and simple (linux-only) vulkan-project template.

## Build

To build you will need to have vulkan and libevdev installed.
On debian systems simply run `sudo apt install libvulkan-dev
vulkan-validationlayers libevdev-dev libx11-dev libglm-dev`.
Then `make -j' to compile the binary.

## Run

Right now only it's possible to control the camera view using a 
game controller. 
If you have one connected, you should be able to find it by issueing
`cat /proc/bus/input/devices` in a terminal.
At some point, you should see something similar to:

``` text
I: Bus=0005 Vendor=054c Product=0268 Version=8000
N: Name="Sony PLAYSTATION(R)3 Controller"
P: Phys=34:de:1a:75:70:e6
S: Sysfs=/devices/pci0000:00/0000:00:14.0/usb2/2-4/2-4:1.0/bluetooth/hci0/hci0:256/0005:054C:0268.0007/input/input44
U: Uniq=00:21:4f:9c:a0:05
H: Handlers=event26 js0 
B: PROP=0
B: EV=20001b
B: KEY=f00000000 0 0 0 7fdb000000000000 0 0 0 0
B: ABS=3f
B: MSC=10
B: FF=107030000 0
```

In the `Handlers:` field, there's `event26` which is name of the character
devices mapped by evdev. In my case then:

```
./space --gamepad=/dev/input/event26
```

will allow you to control the camera using the gamepad.
