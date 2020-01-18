# space
A small and simple vulkan-project template.

## Build

To build you will need to have vulkan and libevdev installed.
On debian systems simply run `sudo apt install libvulkan-dev vulkan-validationlayers libevdev-dev`.
Then `make -j' to compile the binary.

## Run

```
./space --gamepad=/dev/input/event15
```

For now it simply renders a 3d small square to give a reference point.
