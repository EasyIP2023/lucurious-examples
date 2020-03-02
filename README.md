# Lucurious Examples
Folder with a dump of very simple examples for liblucurious.so.

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
meson build
sudo ninja install -C build

# OR
# Encase of PolicyKit daemon errors
pkttyagent -p $(echo $$) | pkexec ninja install -C $(pwd)/build/
```

**To Uninstall**
```bash
sudo ninja uninstall -C build

# OR
# Encase of PolicyKit daemon errors
pkttyagent -p $(echo $$) | pkexec ninja uninstall -C $(pwd)/build/
```

**Usage:**

Lucurious has two different methods got getting SPIR-V bytes **libshaderc** and **wlu_read_file()**.

**When using actual shader files** compile them yourself before use, using ```glslangValidator```.
```bash
glslangValidator -V shaders/shader.frag
glslangValidator -V shaders/shader.vert
```

```bash
cc -Wall -Wextra `pkgconf --cflags --libs lucurious` simple_example.c -o se
./se
```
```bash
make
./se
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
