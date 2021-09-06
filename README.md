# Lucurious Examples

NOTE: The wayland client code is a modified version of what [Simon Ser did here](https://github.com/emersion/hello-wayland/). It will be removed.

Folder with a dump of very simple examples for liblucurious.so.

## Known Issues
* kms-novulkan crashes on (Arch Linux, Ubuntu 20.04)
* rotate-rectangle example crashes on Ubuntu 20.04

### To install

**Via AUR**
```bash
yay -S lucurious-git
```

**Via Github folder**
```bash
git clone https://github.com/EasyIP2023/lucurious.git
cd lucurious/
mkdir -v build
meson --warnlevel=0 build
sudo ninja install -C build

# OR: Encase of PolicyKit daemon errors
pkttyagent -p $(echo $$) | pkexec ninja install -C $(pwd)/build/
```

**To Uninstall**
```bash
sudo ninja uninstall -C build

# OR: Encase of PolicyKit daemon errors
pkttyagent -p $(echo $$) | pkexec ninja uninstall -C $(pwd)/build/
```

**Usage:**

Lucurious has two different methods for getting SPIR-V bytes **libshaderc** and **dlu_read_file()**.

**When using actual shader files** compile them yourself before use, using ```glslangValidator```.

```bash
glslangValidator -V shaders/shader.frag
glslangValidator -V shaders/shader.vert
```

```bash
make
./se

# if running kms examples
./se <optional image>
```

**Command Line Usage**

Print help message
```bash
lucur --help
```
Print instance extensions
```bash
lucur --pie
```
