# Lucurious Examples
Folder with a dump of very simple examples for liblucurious.so.

**To install**
Via AUR
```bash
yay -S lucurious-git
```

Via github folder
```bash
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

**When using acutal SPIR-V files** compile them yourself before use, using ```glslangValidator```. Lucurious has two different methods of getting SPIR-V bytes libshaderc and wlu_read_file().

**NOTE:** If memory leaks are in issue use glslangValidator to compile then wlu_read_file() to read the spirv bytes.
```bash
glslangValidator -V shaders/shader.frag
glslangValidator -V shaders/shader.vert
```

```bash
cc -Wall -Wextra `pkgconf --cflags --libs lucurious` simple_example.c -o se
./se
```
OR!!!!!
```bash
make
./se
```

**Commands Line Usage**

Print help message
```bash
lucur --help
```
Print instance extensions
```bash
lucur --pie
```
