# Building MINI404 in Docker

Ubuntu 20.04 host is too old for QEMU 9.2.1 (needs glib >= 2.66, Focal has 2.64).
Solution: build inside an Ubuntu 24.04 container, run on host with X11 forwarding.

## Architecture

```
Host (Ubuntu 20.04)                    Docker (Ubuntu 24.04)
┌─────────────────────┐               ┌──────────────────────┐
│ sim/                │  bind mount   │ /workspace/sim/      │
│   MINI404/ (source) │──────────────>│   MINI404/ (source)  │
│   fake_puppies/     │               │   fake_puppies/      │
│   xl/, mk4/         │               │   xl/, mk4/          │
│                     │               │   build/ (output)    │
│ X11 display :0      │<── socket ───>│   DISPLAY=:0         │
│ /tmp/PXL_* sockets  │<── shared ───>│   /tmp/PXL_*         │
└─────────────────────┘               └──────────────────────┘
```

Build happens in container. The binary runs from host (via LD path)
or from inside the container. Fake puppies run on host (Python, no
glib dependency).

## Prerequisites

Your terminal must see the docker group (you were added recently):

    newgrp docker
    # or just log out and back in

Verify:

    docker run --rm hello-world

## Step 1: Build the Docker image

    cd ~/prusa/sim/MINI404/docker
    docker build -t mini404-build .

This creates an image with all QEMU build dependencies. Takes ~2-3 min
on first build, cached after that.

## Step 2: Run an interactive build shell

    cd ~/prusa/sim
    docker run --rm -it \
      -v "$PWD":/workspace/sim \
      -w /workspace/sim/MINI404 \
      -e DISPLAY=$DISPLAY \
      -v /tmp/.X11-unix:/tmp/.X11-unix \
      -v /tmp:/tmp \
      --net=host \
      mini404-build bash

## Step 3: Configure and build (inside container)

    ./configure --target-list=buddy-softmmu --disable-werror
    cd build
    ninja

The binary lands at `build/buddy-softmmu/qemu-system-buddy` — visible
from the host via the bind mount.

## Step 4: Run the simulator

### Option A: Run QEMU from inside the container

    # Still inside the container:
    cd /workspace/sim/xl
    # Run directly — X11 forwarding lets GTK work
    ./run_dummy_all.sh

Note: the run scripts hardcode /home/drracer paths. Either:
- Symlink inside container: ln -s /workspace /home/drracer
- Or edit the scripts to use relative paths

### Option B: Run QEMU on the host with container libs

Build inside Docker, then run on host with the container's libs:

    # On host — extract runtime libs from the container image:
    docker run --rm mini404-build tar cf - \
      /usr/lib/x86_64-linux-gnu/libglib-2.0.so* \
      /usr/lib/x86_64-linux-gnu/libgio-2.0.so* \
      /usr/lib/x86_64-linux-gnu/libgobject-2.0.so* \
      /usr/lib/x86_64-linux-gnu/libgmodule-2.0.so* \
      /usr/lib/x86_64-linux-gnu/libgtk-3.so* \
      /usr/lib/x86_64-linux-gnu/libgdk-3.so* \
      | tar xf - -C /tmp/mini404-libs/

    # Then run with:
    LD_LIBRARY_PATH=/tmp/mini404-libs/usr/lib/x86_64-linux-gnu \
      ./MINI404/build/buddy-softmmu/qemu-system-buddy ...

This is fragile (library version tangles). Option A is recommended.

## Convenience: one-liner build

    cd ~/prusa/sim
    docker run --rm \
      -v "$PWD":/workspace/sim \
      -w /workspace/sim/MINI404 \
      mini404-build \
      bash -c './configure --target-list=buddy-softmmu --disable-werror && cd build && ninja -j$(nproc)'

## Convenience: one-liner run (XL with dummy puppies)

    cd ~/prusa/sim
    docker run --rm -it \
      -v "$PWD":/workspace/sim \
      -v /home/drracer/prusa/DR-private:/home/drracer/prusa/DR-private:ro \
      -w /workspace/sim/xl \
      -e DISPLAY=$DISPLAY \
      -v /tmp/.X11-unix:/tmp/.X11-unix \
      -v /tmp:/tmp \
      --net=host \
      mini404-build \
      ./run_dummy_all.sh

## Notes

- **Build artifacts**: `build/` dir is inside the bind-mounted source tree,
  so builds persist across container restarts. `ninja` only rebuilds
  changed files.
- **X11 auth**: If GTK fails with "cannot open display", run `xhost +local:`
  on the host first. (This allows any local user — fine for a dev machine.)
- **/tmp sharing**: The run scripts use /tmp/PXL_* unix sockets for
  QEMU <-> puppy communication. Sharing /tmp lets fake puppies run on
  the host while QEMU runs in the container (or vice versa).
- **GDB debugging**: Add `-p 1234:1234` (or use `--net=host`) to expose
  GDB port.
- **Rebuilds**: Source edits on host are instantly visible in container.
  Just run `ninja` again.
