# I/O

虽然图灵机并没有定义输入输出功能，但是人类要想使用计算机，输入输出就是必需的了。输入输出都是通过访问I/O设备来完成的

## Device & CPU

就像在嵌入式系统中学过的一样，设备接收信号，根据信号的指导来工作。设备具有状态寄存器和功能部件，其工作就是接受命令字，然后进行译码和执行，就像 CPU 一样。

这样，访问设备就是从设备获取数据，或向设备发送数据；除此之外，还需要对设备进行控制，即读取设备的状态寄存器。

>   访问设备 = 读出数据 + 写入数据 + 控制状态

CPU 通过 I/O 编址，访问约定的设备地址来与设备进行交互

## Port-mapped I/O

CPU 使用专门的 I/O 指令对设备进行访问，并把设备的地址称作端口号。有了端口号以后，在 I/O 指令中给出端口号，就知道要访问哪一个设备寄存器了。

## MMIO

端口映射 I/O 会存在地址空间不足的问题，为了更大的编址范围，需要新的编址方式——内存映射 I/O（Memory-mapped I/O）。

MMIO 是通过不同的物理内存地址给设备编址的，将一部分物理内存的访问"重定向"到I/O地址空间中，CPU尝试访问这部分物理内存的时候，实际上最终是访问了相应的I/O设备。

>   虽然这种编址方式会使 CPU 无法再直接访问到这段物理地址上内存了，但是由于存储技术发展飞速，内存空间已经巨大到并不关心减少的这部分内存的程度了。

# NEMU I/O

## Map

框架代码为映射定义了一个结构体类型`IOMap`(在`nemu/include/device/map.h`中定义)

```c
typedef struct {
  const char *name;
  // we treat ioaddr_t as paddr_t here
  paddr_t low;
  paddr_t high;
  void *space;		// 映射的目标空间
  io_callback_t callback;
} IOMap;
```

在`nemu/src/device/io/map.c`实现了映射的管理，包括I/O空间的分配及其映射，还有映射的访问接口。`io_space` 和 `p_space` 用于管理IO空间和普通内存空间。`new_space` 为内存分配新的空间，并确保分配的内存按页对齐。`init_map` 初始化内存映射区域。最后 `map_read` 和 `map_write` 对映射空间进行读写，回调函数在需要更新目标空间的状态时触发。回过头阅读 `paddr_read()` 和 `paddr_write()` 的源码，会发现进行了一个地址判断，看 `addr` 落在物理内存空间还是设备空间，选择调用 `pmem_read()` 和 `pmem_write()` 来访问真正的物理内存还是通过 `map_read()` 和 `map_write()` 来访问相应的设备。

`nemu/src/device/io/port-io.c`是对端口映射I/O的模拟。

## Device

NEMU 实现了串口，时钟，键盘，VGA，声卡，磁盘，SD卡七种设备，需要在menuconfig选中相关选项:

```config
[*] Devices  --->
```

NEMU 使用 SDL 库来实现设备的模拟。`nemu/src/device/device.c `含有和SDL库相关的代码，主要是进行设备的初始化，以及实现设备状态更新和清空事件队列的功能。

# IOE

设备访问的具体实现是架构相关的。由于它提供计算机输入输出功能，将其称为 IOE（I/O Extension）。IOE提供三个API:

```c
bool ioe_init();
void ioe_read(int reg, void *buf);
void ioe_write(int reg, void *buf);
```

我们知道寄存器结构是架构相关的，然而在 AM 中，我们希望提供一个架构无关的 API，便于不同架构的访问。因此函数参数中的 reg 实际上是设备编号，而非实际的寄存器编号，其对应的功能在 `abstract-machine/am/include/amdev.h` 定义。在进行架构有关的实现时，只需要遵守约定即可。

klib中提供了 `io_read() `和 `io_write()` 这两个宏，它们分别对 `ioe_read()` 和 `ioe_write() `这两个API进行了进一步的封装。

# Timer

`nemu/src/device/timer.c`模拟了i8253计时器的功能。i8253计时器初始化时会分别注册`0x48`处长度为8个字节的端口，以及`0xa0000048`处长度为8字节的MMIO空间，它们都会映射到两个32位的RTC寄存器。CPU可以访问这两个寄存器来获得用64位表示的当前时间。

>   `abstract-machine/am/include/amdev.h`中为时钟的功能定义了两个抽象寄存器:
>
>   -   `AM_TIMER_RTC`，AM实时时钟(RTC，Real Time Clock)，可读出当前的年月日时分秒。PA中暂不使用.
>   -   `AM_TIMER_UPTIME`，AM系统启动时间，可读出系统启动后的微秒数.
>
>   RTC泛指流逝速率与真实时间一致的时钟，用户可根据RTC进行一段时间的测量。按照这个定义，上述两个AM抽象寄存器都属于RTC，不过它们的侧重点有所不同: `AM_TIMER_RTC`强调读出的时间与现实时间完全一致，`AM_TIMER_UPTIME`则侧重系统启动后经过的时间，即从0开始计数.

这里在`abstract-machine/am/src/platform/nemu/ioe/timer.c`中实现`AM_TIMER_UPTIME`的功能。我们需要用到在`abstract-machine/am/src/platform/nemu/include/nemu.h`和 `abstract-machine/am/src/$ISA/$ISA.h`中的一些输入输出相关的代码：

```c
void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uint32_t high = inl(RTC_ADDR + 4);
  uint32_t low = inl(RTC_ADDR);
  uptime->us = (uint64_t)low + (((uint64_t)high) << 32);
}
```

其中 `inl` 读取对应地址的4字节数据，分别读取两个32位寄存器后进行组合得到64位时间。`RTC_ADDR` 则是 MMIO 空间的地址，系统调用 `vaddr` 再调用 `paddr` 后会发现位于 MMIO 空间，则调用 `mmio_read` 进行设备访问。

>   **看看NEMU跑多快**
>
>   在`am-kernel/benchmarks/`目录下可以运行不同复杂度的 benchmark 进行系统跑分。在真机上通过RISC-V的NEMU(按照PA的常规流程开发，未进行优化)运行microbench的`ref`规模输入，跑分约300~500。
>
>   框架代码中埋了一些小坑，会导致跑分出现问题。在 `nemu/src/device/timer.c` 的 `rtc_io_handler` 中：
>
>   ```c
>   static void rtc_io_handler(uint32_t offset, int len, bool is_write) {
>     assert(offset == 0 || offset == 4);
>     if (!is_write && offset == 4) {
>       uint64_t us = get_time();
>       rtc_port_base[0] = (uint32_t)us;
>       rtc_port_base[1] = us >> 32;
>     }
>   }
>   ```
>
>   这里会发现执行了一个 `offset == 4` 的判断，来处理两次访问寄存器时避免重复读取时间的问题。如果我们第一次读取的是低32位的时间，那么条件为假，导致 `rtc_port_base` 没有更新，这样获取到的是上一次`__am_timer_uptime`得到的系统时间的低32位，导致跑分出现问题。有两种修改方法：
>
>   -   `rtc_io_handler`中的判断改成`offset==0`
>   -   `__am_timer_uptime`中先获取高32位

# dtrace

类似 `mtrace` ，在 `mmio_read` 和 `mmio_write` 时调用 `dtrace` 进行记录。

```c
#include <common.h>
#include <device/map.h>

#define dtrace_write log_write

void trace_dread(paddr_t addr, int len, IOMap *map) {
	dtrace_write("dtrace: read %10s at " FMT_PADDR ",%d\n",
		map->name, addr, len);
}

void trace_dwrite(paddr_t addr, int len, word_t data, IOMap *map) {
	dtrace_write("dtrace: write %10s at " FMT_PADDR ",%d with " FMT_WORD "\n",
		map->name, addr, len, data);
}
```



```c
// nemu/src/device/io/mmio.c
word_t mmio_read(paddr_t addr, int len) {
  return map_read(addr, len, fetch_mmio_map(addr));
}

void mmio_write(paddr_t addr, int len, word_t data) {
  map_write(addr, len, data, fetch_mmio_map(addr));
}
```

# keyboard

在嵌入式课程中我们已经有过按键信号的处理经验了：在按下按键时，键盘发送通码（make code），释放时发送断码（break code）。`nemu/src/device/keyboard.c`模拟了i8042通用设备接口芯片的功能。

>   i8042芯片在初始化时，会在地址0x60处注册一个长度为4字节的端口，并在地址0xa0000060处注册一个长度为4字节的 MMIO 空间，它们都映射到i8042的数据寄存器。当用户按下或释放一个键时，i8042会将相应的键盘扫描码放入数据寄存器。CPU 可以通过访问这些地址读取数据寄存器中的键盘码。如果没有按键输入，数据寄存器会返回 `AM_KEY_NONE`，表示当前没有可用的键盘码。

我们需要在`abstract-machine/am/src/platform/nemu/ioe/input.c`中实现`AM_INPUT_KEYBRD`的功能，也就是`keydown`为`true`时表示按下按键，否则表示释放按键。参考 `native` 中的对应实现，除了使用 SDL 提供的互斥锁保证键盘事件队列的访问是线程安全的之外，只需要使用按键掩码获取按键状态即可：

```c
void __am_input_keybrd(AM_INPUT_KEYBRD_T *kbd) {
  uint32_t kc = inl(KBD_ADDR);
  kbd->keydown = kc & KEYDOWN_MASK ? true : false;
  kbd->keycode = kc & ~KEYDOWN_MASK;
}
```

>   **Multikey pressed**
>
>   游戏中的许多情况还需要判断玩家是否同时按下多个键来做出反应，比如 RPG 游戏八向移动或格斗游戏组合技等。
>
>   根据查到的资料，社区给出的实现主要是使用 `switch case` 语句或类似维护一个 `isReleased` 数组对接收到的特定的组合键进行判断，也符合实际开发环境中只有特定的几个按键组合有实际作用的情况。

# VGA

VGA 可以用于显示颜色像素，`nemu/src/device/vga.c`模拟了 VGA 的功能。这些设备的初始化都是需要注册特定地址的一段内存，用于映射到设备的 MMIO 空间。VGA 注册了从`0xa1000000`开始的一段用于映射到显存（Video Memory，或帧缓冲 frame buffer）的 MMIO 空间。[**Hardware Level VGA and SVGA Video Programming Information Page**](https://www.scs.stanford.edu/10wi-cs140/pintos/specs/freevga/home.htm) 给出了一些 VGA 编程的资料。

>   在AM中，显示相关的设备叫GPU，GPU是一个专门用来进行图形渲染的设备。在NEMU中，我们并不支持一个完整GPU的功能，而仅仅保留绘制像素的基本功能.
>
>   `abstract-machine/am/include/amdev.h`中为GPU定义了五个抽象寄存器，在NEMU中只会用到其中的两个:
>
>   -   `AM_GPU_CONFIG`，AM显示控制器信息，可读出屏幕大小信息`width`和`height`。另外AM假设系统在运行过程中，屏幕大小不会发生变化.
>   -   `AM_GPU_FBDRAW`，AM帧缓冲控制器，可写入绘图信息，向屏幕`(x，y)`坐标处绘制`w*h`的矩形图像。图像像素按行优先方式存储在`pixels`中，每个像素用32位整数以`00RRGGBB`的方式描述颜色。若`sync`为`true`，则马上将帧缓冲中的内容同步到屏幕上.

**VGA设备** 的寄存器有**屏幕大小寄存器**和**同步寄存器**：屏幕大小寄存器通常用于设置显示屏的分辨率或屏幕大小，比如控制显示区域的宽度和高度。这个寄存器会保存有关显示屏尺寸的信息，硬件根据这些信息来渲染屏幕上的内容；同步寄存器通常用于控制显示器的刷新和同步操作，例如控制屏幕的垂直和水平同步信号。这些信号确保屏幕的内容正确地按帧刷新，从而避免图像的撕裂或不同步。

NEMU 给出了屏幕大小寄存器的硬件实现，AM 给出了同步寄存器的软件实现，我们需要分别实现这两个寄存器各自的软硬件实现。

`__am_gpu_config` 中主要做的就是对屏幕宽和高进行设置，阅读 `nemu/src/device/vga.c` 中相关代码：

```c
void init_vga() {
  vgactl_port_base = (uint32_t *)new_space(8);
  vgactl_port_base[0] = (screen_width() << 16) | screen_height();
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("vgactl", CONFIG_VGA_CTL_PORT, vgactl_port_base, 8, NULL);
#else
  add_mmio_map("vgactl", CONFIG_VGA_CTL_MMIO, vgactl_port_base, 8, NULL);
#endif
```

可以知道屏幕的宽和高存储在了 `vgactl_port_base[0]`，其中高16位存储宽度，低16位存储高度，`abstract-machine/am/src/platform/nemu/include/nemu.h` 中定义了其存储的地址为 `VGACTL_ADDR`，这样就可以实现 `__am_gpu_config` 中对屏幕宽高的设置。

```c
// abstract-machine/am/src/platform/nemu/ioe/gpu.c
void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  uint32_t screen_wh = inl(VGACTL_ADDR);
  uint32_t h = screen_wh & 0xffff;
  uint32_t w = screen_wh >> 16;
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = w, .height = h,
    // .width = inw(VGACTL_ADDR + 2), .height = inw(VGACTL_ADDR),
    .vmemsz = 0
  };
}
```

接下来需要实现 NEMU 框架中的同步寄存器功能。在 `nemu/src/device/vga.c` 中可以看到 TODO 提示的 `vga_update_screen` 函数。文档中已经提示 `abstract-machine/am/src/platform/nemu/ioe/gpu.c` 已经实现了 `AM_GPU_FBDRAW` 的软件部分，只需要外界调用`io_write(AM_GPU_FBDRAW, ... ,true)`时（最后一个参数是sync），用outb输出到抽象寄存器即可（事实上并未正确实现）。

NEMU 中的硬件部分的实现参考 TODO 的提示：

```c
// nemu/src/device/vga.c
void vga_update_screen() {
  // TODO: call `update_screen()` when the sync register is non-zero,
  // then zero out the sync register
  uint32_t sync = vgactl_port_base[1];
  if (sync) {
    update_screen();
    vgactl_port_base[1] = 0;
  }
}
```

主要进行的工作是检查 VGA 控制寄存器中的同步寄存器，如果该寄存器的值为非零，则调用 `update_screen()` 函数来更新屏幕内容。更新完成后，将同步寄存器的值清零，以便下次更新时可以再次检测到。这样可以确保屏幕只在需要时进行更新，避免不必要的刷新操作。

在`$ISA-nemu`中运行`am-tests`中的`display test`测试，会发现输出了一些颜色信息，但是这时还没有完全正确实现 `AM_GPU_FBDRAW`，它只是把需要绘制的区域的像素点数据放到 frame buffer 中，最终是在 vga.c 的每次设备更新中从 frame buffer 中取出数据并实现绘制。接下来完善 `__am_gpu_fbdraw`。

注意到 `AM_GPU_FBDRAW_T` 的定义：

```c
// abstract-machine/am/include/amdev.h
#define AM_DEVREG(id, reg, perm, ...) \
  enum { AM_##reg = (id) }; \
  typedef struct { __VA_ARGS__; } AM_##reg##_T;
AM_DEVREG(11, GPU_FBDRAW,   WR, int x, y; void *pixels; int w, h; bool sync);
```

另外根据测试代码中 `io_write(AM_GPU_FBDRAW, x * w, y * h, color_buf, w, h, false);` 这段调用语句，可以看到传入的参数与 `__am_gpu_fbdraw` 中的参数一一对应：

-   x：绘制的水平起始点
-   y：绘制的垂直起始点
-   w：绘制的矩形宽度
-   h：绘制的矩形高度
-   pixels：绘制的矩形内所有像素点的颜色，表示成二维数组就是，`pixels[i][j]` 表示点 `(i+x, j+y) `的颜色，这个坐标是相对整个 GUI 程序来说的，即 GUI 程序的左上角点坐标为 `(0, 0)`

明白了函数的行为之后，实现就很简单了。

```c
void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  int x = ctl->x, y = ctl->y, w = ctl->w, h = ctl->h;
  if (!ctl->sync && (w == 0 || h == 0)) return;
  uint32_t *pixels = ctl->pixels;
  uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;
  uint32_t screen_w = inl(VGACTL_ADDR) >> 16;
  for (int i = y; i < y+h; i++) {
    for (int j = x; j < x+w; j++) {
      fb[screen_w*i+j] = pixels[w*(i-y)+(j-x)];
    }
  }
  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}
```

# Audio Card

>   在NEMU中，我们根据SDL库的API来设计一个简单的声卡设备。使用SDL库来播放音频的过程非常简单:
>
>   1.  通过`SDL_OpenAudio()`来初始化音频子系统，需要提供频率，格式等参数，还需要注册一个用于将来填充音频数据的回调函数 更多的信息请阅读`man SDL_OpenAudio`(需要安装`libsdl2-doc`)或者[这个页面](https://wiki.libsdl.org/SDL2/SDL_OpenAudio).
>   2.  SDL库会定期调用初始化时注册的回调函数，并提供一个缓冲区，请求回调函数往缓冲区中写入音频数据
>   3.  回调函数返回后，SDL库就会按照初始化时提供的参数来播放缓冲区中的音频数据

下面进行一点 RTFSC：

首先定义了一些寄存器：

```c
enum {
  reg_freq,			// 采样频率
  reg_channels,		// 通道数
  reg_samples,		// 样本数
  reg_sbuf_size,	// 流缓冲区
  reg_init,			// 初始化
  reg_count,		// 当前流缓冲区已经使用的大小
  nr_reg
};
```

在AM中，`abstract-machine/am/include/amdev.h`中为声卡定义了四个抽象寄存器:

-   `AM_AUDIO_CONFIG`，AM声卡控制器信息，可读出存在标志`present`以及流缓冲区的大小`bufsize`。另外AM假设系统在运行过程中，流缓冲区的大小不会发生变化.
-   `AM_AUDIO_CTRL`，AM声卡控制寄存器，可根据写入的`freq`，`channels`和`samples`对声卡进行初始化.
-   `AM_AUDIO_STATUS`，AM声卡状态寄存器，可读出当前流缓冲区已经使用的大小`count`.
-   `AM_AUDIO_PLAY`，AM声卡播放寄存器，可将`[buf.start，buf.end)`区间的内容作为音频数据写入流缓冲区。若当前流缓冲区的空闲空间少于即将写入的音频数据，此次写入将会一直等待，直到有足够的空闲空间将音频数据完全写入流缓冲区才会返回.

## RTFSC

`am-kernels/tests/am-tests/src/tests/audio.c` 中展示了声卡IOE抽象的使用方式。根据下面的代码，实际上 IOE 抽象以及大多数音视频设备的使用方式基本都是进行配置、初始化，获取地址，然后进入一个 `while` 循环进行执行。执行过程中将缓冲区数据写入设备即可。

```c
void audio_test() {
  // 检查音频设备是否存在
  if (!io_read(AM_AUDIO_CONFIG).present) {
    printf("WARNING: %s does not support audio\n", TOSTRING(__ARCH__));
    return;
  }

  // 配置音频设备：采样率8000Hz，单声道，缓冲区大小1024字节
  io_write(AM_AUDIO_CTRL, 8000, 1, 1024);

  // 获取音频数据的起始和结束地址
  extern uint8_t audio_payload, audio_payload_end;
  // 计算音频数据长度
  uint32_t audio_len = &audio_payload_end - &audio_payload; 
  int nplay = 0; // 已播放的数据长度
  Area sbuf;
  sbuf.start = &audio_payload; // 初始化缓冲区起始地址

  // 循环播放音频数据
  while (nplay < audio_len) {
    // 计算当前要播放的数据长度，最多4096字节
    int len = (audio_len - nplay > 4096 ? 4096 : audio_len - nplay);
    sbuf.end = sbuf.start + len; // 设置缓冲区结束地址
    io_write(AM_AUDIO_PLAY, sbuf); // 将缓冲区数据写入音频设备进行播放
    sbuf.start += len; // 更新缓冲区起始地址
    nplay += len; // 更新已播放的数据长度
    printf("Already play %d/%d bytes of data\n", nplay, audio_len); // 打印已播放的数据长度
  }

  // 等待音频播放完成
  while (io_read(AM_AUDIO_STATUS).count > 0);
}
```

>   TODO：尚未搞明白这些宏定义是怎么做到类似 Higher Order Function 被调用的，日后研究。

---

## NEMU-audio

`AudioSpec_` 定义音频处理的初始化参数，分别代表采样频率（frequency）、通道数（channels）、样本数（samples），另外定义音频播放标志、流缓冲区的各项数据以及音频设备的基址，并初始化。首先进行硬件实现：

```c
// nemu/src/device/audio.c
static bool audio_opened = false;

static uint8_t *sbuf = NULL;
static size_t sbuf_pos = 0;
static size_t sbuf_count = 0;
static uint32_t *audio_base = NULL;

typedef struct AudioSpec_ {
  uint32_t freq;
  uint32_t channels;
  uint32_t samples;
} AudioSpec_t;

static AudioSpec_t audio_spec;


```

初始化完成后，只需要维护流缓冲区即可。我们可以把流缓冲区可以看成是一个队列，程序通过`AM_AUDIO_PLAY`的抽象往流缓冲区里面写入音频数据，而SDL库的回调函数则从流缓冲区里面读出音频数据（实际上有点像 CS144 lab0 实现的 ByteStream，不过流式结构应该都是这么用的）。使用 SDL 提供的函数初始化 SDL 音频子系统并打开音频设备，然后设置音频规格并注册回调函数 `callback_play`。

>   SDL 提供了相关函数的文档网站（这里使用的是 SDL2，现在已更新到 SDL3）
>
>   [SDL_InitSubSystem](https://wiki.libsdl.org/SDL2/SDL_InitSubSystem)、[SDL_OpenAudio](https://wiki.libsdl.org/SDL2/SDL_OpenAudio)、[SDL_PauseAudio](https://wiki.libsdl.org/SDL2/SDL_PauseAudio)

```c
static void do_sdl_audio_init(void) {
  sbuf_pos = 0;
  sbuf_count = 0;

  SDL_AudioSpec want = {
      .freq = audio_spec.freq,
      .channels = audio_spec.channels,
      .samples = audio_spec.samples,
      .format = AUDIO_S16SYS,
      .userdata = NULL,
      .callback = callback_play,
  };

  if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    Warn("%s", SDL_GetError());
  }
  if (SDL_OpenAudio(&want, NULL)) {
    Warn("%s", SDL_GetError());
  }
  SDL_PauseAudio(0);

  audio_opened = true;
}
```

实现回调函数，用于将缓冲区中的内容复制到设备寄存器中，另外需要处理缓冲区末尾到达缓冲区起始位置的情况，也就是文档中提到的：如果回调函数需要的数据量大于当前流缓冲区中的数据量，你还需要把SDL提供的缓冲区剩余的部分清零，以避免把一些垃圾数据当做音频，从而产生噪音。

```c
static void callback_play(void *user_data, uint8_t *stream, int len) {
  memset(stream, 0, len);
  const size_t chunk_len = (size_t)len > sbuf_count ? sbuf_count : (size_t)len;
  if ((sbuf_pos + chunk_len) > CONFIG_SB_SIZE) {
    const uint32_t len_rem = CONFIG_SB_SIZE - sbuf_pos;
    SDL_MixAudio(stream, sbuf + sbuf_pos, len_rem, SDL_MIX_MAXVOLUME);
    SDL_MixAudio(stream + len_rem, sbuf, chunk_len - len_rem, SDL_MIX_MAXVOLUME);
  } else {
    SDL_MixAudio(stream, sbuf + sbuf_pos, chunk_len, SDL_MIX_MAXVOLUME);
  }
  sbuf_pos = (sbuf_pos + chunk_len) % CONFIG_SB_SIZE;
  sbuf_count -= chunk_len;
}
```

 `audio_io_handler` 用于处理音频控制寄存器的读写操作（`<device>_io_handler` 在框架代码的许多设备的实现中都能看到），根据寄存器的偏移量执行相应的操作，如设置频率、声道、样本数，初始化音频设备。`audio_sbuf_handler` 只需要处理音频缓冲区的写操作，更新缓冲区中的数据量。

```c
static void audio_io_handler(uint32_t offset, int len, bool is_write) {
  assert(len == 4);
  switch (offset / sizeof(uint32_t)) {
    case reg_freq:
      if (is_write) {
        audio_spec.freq = audio_base[reg_freq];
      }
      break;
    case reg_channels:
      if (is_write) {
        audio_spec.channels = audio_base[reg_channels];
      }
      break;
    case reg_samples:
      if (is_write) {
        audio_spec.samples = audio_base[reg_samples];
      }
      break;
    case reg_sbuf_size:
      if (!is_write) {
        audio_base[reg_sbuf_size] = CONFIG_SB_SIZE;
      }
      break;
    case reg_init:
      if (is_write && audio_base[reg_init]) {
        if (audio_opened) {
          do_sdl_audio_shutdown();
        }
        do_sdl_audio_init();
        audio_base[reg_init] = 0;
      }
      break;
    case reg_count:
      if (!is_write) {
        audio_base[reg_count] = (uint32_t)sbuf_count;
      }
      break;
    default: panic("do not support offset = %d", offset);
  }
}

static void audio_sbuf_handler(uint32_t offset, int len, bool is_write) {
  if (likely(is_write)) {
    sbuf_count += len;
  }
}
```

定义 `do_sdl_audio_shutdown` 关闭 SDL 音频子系统并释放资源。

```c
static void do_sdl_audio_shutdown(void) {
  SDL_PauseAudio(1);
  SDL_CloseAudio();
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  audio_opened = false;
}
```

最后在 `init_audio` 函数中修改这条语句，添加我们实现的 `audio_sbuf_handler` 即可：

```c
add_mmio_map("audio-sbuf", CONFIG_SB_ADDR, sbuf, CONFIG_SB_SIZE, audio_sbuf_handler);
```

## AM-audio

软件部分，IOE 提供了所需的 API，我们只需要根据各自的功能进行实现。虽然文档没有给出每个 API 的功能，但是 `abstract-machine/am/include/amdev.h` 中定义了这里用到的各个宏：

```c
// abstract-machine/am/include/amdev.h
AM_DEVREG(14, AUDIO_CONFIG, RD, bool present; int bufsize);
AM_DEVREG(15, AUDIO_CTRL,   WR, int freq, channels, samples);
AM_DEVREG(16, AUDIO_STATUS, RD, int count);
AM_DEVREG(17, AUDIO_PLAY,   WR, Area buf);
```

我们要做的工作就是正确设置这里体现的参数。

```c
// abstract-machine/am/src/platform/nemu/ioe/audio.c
static size_t sbuf_pos = 0;
static uint32_t sbuf_size = 0;

void __am_audio_init() {
  sbuf_pos = 0;
  sbuf_size = inl(AUDIO_SBUF_SIZE_ADDR);
}

void __am_audio_config(AM_AUDIO_CONFIG_T *cfg) {
  cfg->present = true;
  cfg->bufsize = sbuf_size;
}

void __am_audio_ctrl(AM_AUDIO_CTRL_T *ctrl) {
  outl(AUDIO_FREQ_ADDR, ctrl->freq);
  outl(AUDIO_CHANNELS_ADDR, ctrl->channels);
  outl(AUDIO_SAMPLES_ADDR, ctrl->samples);

  outl(AUDIO_INIT_ADDR, 1);
}

void __am_audio_status(AM_AUDIO_STATUS_T *stat) {
  stat->count = inl(AUDIO_COUNT_ADDR);
}

void __am_audio_play(AM_AUDIO_PLAY_T *ctl) {
  const uint16_t *const buf = ctl->buf.start;
  const uint32_t len = (uint16_t *)(ctl->buf.end) - buf;

  for (size_t i = 0; i < len; i++) {
    outw(AUDIO_SBUF_ADDR + sbuf_pos, buf[i]);
    sbuf_pos = (sbuf_pos + 2) % sbuf_size;
  }
}
```

## ffmpeg

ffmpeg 最早由伟大的 [Fabrice Bellard](https://en.wikipedia.org/wiki/Fabrice_Bellard) 开发（Also known for writing QEMU），现在已经是一个很大型的 [Free and open-source software (FOSS)](https://en.wikipedia.org/wiki/Free_and_open-source_software)。我们可以使用下面的命令：

```bash
ffmpeg -i MyMusic.mp3 -acodec pcm_s16le -f s16le -ac 1 -ar 44100 44k.pcm
```

-   `-i` 后面指定输入的音频文件
-   `-acodec pcm_s16le` 指定音频编码器为 `pcm_s16le`，即 16 位的小端格式的 PCM（Pulse Code Modulation）音频数据。这个编码器表示无压缩的音频格式。
-   `-f s16le`：指定输出文件的格式为 `s16le`，也就是 16 位小端格式的音频数据。
-   `-ac 1`：设置音频通道数为 1，意味着输出为单声道（Mono）。
-   `-ar 44100`：设置音频采样率为 44100 Hz，这通常是 CD 音质的采样率。

# Game & IOE

上述的 VGA 和声卡都体现了"程序如何使用IOE来实现游戏效果"的框架，也就是如下所示的死循环：

```c
while (1) {
  等待新的一帧();  // AM_TIMER_UPTIME
  处理用户按键();  // AM_INPUT_KEYBRD
  更新游戏逻辑();  // TRM
  绘制新的屏幕();  // AM_GPU_FBDRAW
}
```

我们对 `am-kernels/kernels/typing-game/game.c` 下的小游戏进行 RTFSC，尝试分析游戏是如何在系统中运行的。首先看 `main` 函数，进行一些初始化后，对设备的 `*_CONFIG.present` 状态进行断言判断是否正确设置。接下来读取时钟信息作为开始时间，用于后面计算帧数。接下来进入死循环，计算当前时间与初始时间 `t0` 之间的时间差，并根据帧率 `FPS` 计算应该更新的帧数 `frames`。`for` 循环体现了 `frames` 的作用：逐帧更新游戏逻辑。回头看到前面定义的 `current` 和 `rendered` 变量，分别用于跟踪游戏逻辑和渲染的帧数。最后在另一个 `while(1)` 中处理按键信号。

注意到这里 `while(1)` 中套了一个 `while(1)`，但是因为 `io_read` 读取到的信息会被立即处理并退出循环，因此不会卡死在键盘处理逻辑的死循环中。

```c
int main() {
  ioe_init();
  video_init();

  panic_on(!io_read(AM_TIMER_CONFIG).present, "requires timer");
  panic_on(!io_read(AM_INPUT_CONFIG).present, "requires keyboard");

  printf("Type 'ESC' to exit\n");

  int current = 0, rendered = 0;
  uint64_t t0 = io_read(AM_TIMER_UPTIME).us;
  while (1) {
    int frames = (io_read(AM_TIMER_UPTIME).us - t0) / (1000000 / FPS);

    for (; current < frames; current++) {
      game_logic_update(current);
    }

    while (1) {
      AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
      if (ev.keycode == AM_KEY_NONE) break;
      if (ev.keydown && ev.keycode == AM_KEY_ESCAPE) halt(0);
      if (ev.keydown && lut[ev.keycode]) {
        check_hit(lut[ev.keycode]);
      }
    };

    if (current > rendered) {
      render();
      rendered = current;
    }
  }
}
```

我们主要关心渲染器 `render` 和游戏逻辑更新函数 `game_logic_update`。可以看到 `game_logic_update` 主要用于游戏本身玩法的实现，如更新字符位置，生成新字符，记录游戏信息等等。`render` 则是用于根据信息渲染游戏画面（这就是[渲染](https://en.wikipedia.org/wiki/Rendering_(computer_graphics))的定义）还会注意到向 VGA 设备写入数据时用到了数组 `texture` ，经常打游戏的话会知道这个词是纹理的意思，在这里则用于绘制各种颜色的不同字母。

```c
void game_logic_update(int frame) {
  if (frame % (FPS / CPS) == 0) new_char();
  for (int i = 0; i < LENGTH(chars); i++) {
    struct character *c = &chars[i];
    if (c->ch) {
      if (c->t > 0) {
        if (--c->t == 0) {
          c->ch = '\0';
        }
      } else {
        c->y += c->v;
        if (c->y < 0) {
          c->ch = '\0';
        }
        if (c->y + CHAR_H >= screen_h) {
          miss++;
          c->v = 0;
          c->y = screen_h - CHAR_H;
          c->t = FPS;
        }
      }
    }
  }
}

void render() {
  static int x[NCHAR], y[NCHAR], n = 0;

  for (int i = 0; i < n; i++) {
    io_write(AM_GPU_FBDRAW, x[i], y[i], blank, CHAR_W, CHAR_H, false);
  }

  n = 0;
  for (int i = 0; i < LENGTH(chars); i++) {
    struct character *c = &chars[i];
    if (c->ch) {
      x[n] = c->x; y[n] = c->y; n++;
      int col = (c->v > 0) ? WHITE : (c->v < 0 ? GREEN : RED);
      io_write(AM_GPU_FBDRAW, c->x, c->y, texture[col][c->ch - 'A'], CHAR_W, CHAR_H, false);
    }
  }
  io_write(AM_GPU_FBDRAW, 0, 0, NULL, 0, 0, true);
  for (int i = 0; i < 40; i++) putch('\b');
  printf("Hit: %d; Miss: %d; Wrong: %d", hit, miss, wrong);
}
```

总结：这个打字小游戏程序完全符合前面的抽象：在死循环中等待（计算）新的一帧，处理输入（按键），更新游戏逻辑，绘制图像。在此之前还需要进行各项设备的初始化，写入所需的配置参数。在更新过程中，主要就是对设备寄存器的读写，这也符合前面所讲的 MMIO ，对于 CPU 来说，访问设备也只是对特定内存区域的读写。

# Summary

这里是必答题的汇总

## YEMU

YEMU 程序是一个极简版本的计算机取指、译码、执行流程。程序定义了指令格式、译码方法、PC、寄存器和内存。`exec_once` 模拟了上述整个流程：取出 PC 位置的指令，根据约定的解码规则进行译码和执行，最后更新 PC。主函数中，进入一个 `while(1)` 一条条地执行指令，我们就实现了一个程序控制执行的简单系统。

## RTFC

一条指令在 NEMU 中的执行流程：在 NEMU 启动后，以批处理模式为例，monitor 调用 `cpu_exec(-1)`，相当于 `while(1)` 不停地执行指令。进入 `cpu_exec` ，其内部代码如下：

```c
static void execute(uint64_t n) {
  Decode s;
  for (;n > 0; n --) {
    exec_once(&s, cpu.pc);
    g_nr_guest_inst ++;
    trace_and_difftest(&s, cpu.pc);
    if (nemu_state.state != NEMU_RUNNING) break;
    IFDEF(CONFIG_DEVICE, device_update());
  }
}
```

可以看到包含一个指令执行全部信息的 `Decode s` 的地址会被传入 `exec_once` ，在这里将会完成一个完整的取指、译码、执行过程。进入 `exec_once`，更新 `s->pc` 和 `s->snpc` 后，会调用 `isa_exec_once`，这里并没有直接实现，是为了将具体架构与指令执行解耦，这样在执行客户程序时就不需要考虑具体的架构实现。在 `isa_exec_once` 中将会调用 `inst_fetch` 进行取指，然后将 `s` 传入 `decode_exec` 进行译码和执行（可以看到这里依旧在进行解耦，是为了优化代码的结构，避免过多的 copy-paste）

>   注意到取指的地址是 `s->snpc` 而非 `s->pc`，查询了一些资料，发现以前对 Fetch 过程的认知有误，实际上取指一直都是取下一条执行的指令，。

在 GDB 中调试 NEMU，可以看到 PC 和 SNPC 一开始都指向 0x80000000 处，也就是 RISC-V32的起始位置。接下来进行取指，也就是取 `s->snpc` ，而 `inst_fetch` 访问内存之后会更新 `s->snpc` 一个指令长度的位置，也就是静态代码文本中的下一指令位置（由于跳转的存在，程序指令执行顺序并不一定等于静态文本中的指令顺序，这也就是维护 `dnpc` 的原因）。取址后会用 `s->snpc` 更新 `s->dnpc`，因为如果不需要跳转，`s->dnpc` 就等于 `s->snpc`，如果需要跳转，则需要对 `dnpc` 进行加减法运算，因此在译码前使用 `snpc` 更新 `dnpc`。

进入译码环节，NEMU 定义了一系列巧妙的宏来完成这个工作。这些宏一方面可以分离译码和执行的环节（未解耦的代码可以去看看 ics2021 之前的项目，非常触目惊心），另外统一的宏虽然提高了阅读难度，但避免了大量的 copy-paste，方便后续的维护和调试，也使代码更加整洁美观。

在译码执行阶段，系统会根据已经实现的指令译码表格逐个匹配，匹配成功时，调用 `decode_oprand`，根据指令类型提取出源操作数、目的寄存器等等数据，执行对应的代码，最后将 `$zero` 寄存器重置为0.

结束上述流程后，实际上已经完成了一次取指、译码、执行了，接下来的工作就是使用 `dnpc` 更新 `pc`，然后继续从客户程序读取指令，进行下一轮执行。

## Running

程序是如何运行的？

还是以打字游戏为例，PA1 中 RTFSC 之后，我们已经知道 NEMU 通过 monitor 调用 `load_img` 装载客户程序。具体地，Makefile 会传入客户程序目标文件的地址，在 `load_img` 中通过 `fopen` 打开文件。

>   在这里遇到了一个有意思的事，在逐步调试时，我忘记打开编译选项的 Device，导致访问 MMIO 空间出现问题，最后 iringbuf 停在了 `c.addi` 指令，但是这个指令我并没有实现，暂时不知道什么情况，以后有时间研究一下。

装载后，由于我们设置了 NEMU 的批处理模式，会自动执行 `cmd_c`，也就是不停地执行指令，接下来我们就很熟悉了，计算机系统就是一个不停执行指令的机器。研究打字小游戏的 `main` 函数，它会调用 AM 提供的 IOE 初始化函数 `ioe_init`，后者将进行 GPU、计时器、声卡的初始化。接下来游戏自身进行视频（Video）设置的初始化，获取屏幕宽高，绘制初始界面，初始化字体纹理数组。完成初始化后就可以进入游戏的 `while(1)` 循环了，在循环中，游戏会根据开始时间计算当前帧数，据此逐帧更新游戏逻辑：生成新字符、字符下落。更新后，进入内部的死循环处理用户的按键输入。由于按键信号被读取后将会立即被处理，所以并不会造成卡死。最后判断渲染画面是否落后当前游戏进度，如果是则重新渲染画面。

## Compile & Link (1)

>   在`nemu/include/cpu/ifetch.h`中, 你会看到由`static inline`开头定义的`inst_fetch()`函数. 分别尝试去掉`static`, 去掉`inline`或去掉两者, 然后重新进行编译, 你可能会看到发生错误. 请分别解释为什么这些错误会发生/不发生? 你有办法证明你的想法吗?

~~我在 Mac 上的 lima 虚拟机里删除两个都能正常运行？？回去用 WSL2 再试一试……~~

去掉两者时会报错：

```bash
/usr/bin/ld: /home/nocturne.linux/Project/XM_ICS2024PA/nemu/build/obj-riscv32-nemu-interpreter/src/isa/riscv32/inst.o: in function `inst_fetch':
/home/nocturne.linux/Project/XM_ICS2024PA/nemu/include/cpu/ifetch.h:20: multiple definition of `inst_fetch'; /home/nocturne.linux/Project/XM_ICS2024PA/nemu/build/obj-riscv32-nemu-interpreter/src/engine/interpreter/hostcall.o:/home/nocturne.linux/Project/XM_ICS2024PA/nemu/include/cpu/ifetch.h:20: first defined here
collect2: error: ld returned 1 exit status
make: *** [/home/nocturne.linux/Project/XM_ICS2024PA/nemu/scripts/build.mk:54: /home/nocturne.linux/Project/XM_ICS2024PA/nemu/build/riscv32-nemu-interpreter] Error 1
```

阅读报错信息即可知道，如果两者都去掉的话，链接器在链接过程中将会发现 `inst_fetch` 函数的多个定义。这是因为如果不加描述地定义函数，而这个头文件又被多个文件包含，就会导致每个源文件都有一个 `inst_fetch` 的定义，从编译器角度来看，会生成多个同名的符号，造成地址的冲突出错。

只去掉其中一个都不会出现问题：如果去掉 `static` ，保留 `inline` 关键字，即声明为内联函数，此时编译器将会其实现被“复制”到每个调用点，编译器不会像普通函数那样生成符号（即函数地址），从而避免了链接器冲突；如果去掉 `static` ，保留 `inline` ，此时将作为静态函数，`static` 将函数的作用域限制为定义它的源文件，这样即使多个源文件定义了同名的 `static` 函数，它们也不会相互冲突。如果同名的 `static` 函数出现在多个源文件中，链接器会将每个源文件中的符号视为独立的符号。

## Compile & Link (2)

>   在`nemu/include/common.h`中添加一行`volatile static int dummy;` 然后重新编译NEMU. 请问重新编译后的NEMU含有多少个`dummy`变量的实体? 你是如何得到这个结果的?

首先，我不知道什么是实体……

>   An entity is an identifier (name in the software text), meant at execution time to denote possible values. Some entities are read-only: the execution can’t change their initial value. Others, called variables, can take on successive values during execution as a result of such operations as creation and assignment.
>
>   ---来自 [ETH](https://se.inf.ethz.ch/~meyer/ongoing/etl/entity.pdf)

查看编译后得到的 ELF 文件 `riscv32-nemu-interpreter`，在符号表中只出现了一个作为 OBJECT 的 `dummy` 符号：

```
000000000000e4d8     4 OBJECT  LOCAL  DEFAULT   27 dummy
```

根据编译得到的文件，在 `build` 中执行

```bash
grep -r -c 'dummy' ./* | grep '\.o:[1-9]' | wc -l
> 29
```

得到所有目标文件中有29个`dummy` 文本，这些应该都是包含了 `common.h` 头文件的 C 文件编译得到的目标文件。使用 `hd` 查看得到的 `.o` 文件的确可以找到 `dummy` 变量文本。

>   添加上题中的代码后, 再在`nemu/include/debug.h`中添加一行`volatile static int dummy;` 然后重新编译NEMU. 请问此时的NEMU含有多少个`dummy`变量的实体? 与上题中`dummy`变量实体数目进行比较, 并解释本题的结果.

依然是29个，不过这是因为包含了 `common.h` 的文件大多也同时包含了 `debug.h`，所以虽然两个头文件都定义了静态的 `dummy` 变量，但是我们知道头文件的包含语句是直接将头文件代码引入源文件，所以这里只是声明了两次 `dummy` 变量，实际上 `dummy` 仍只占有一个地址空间，也就是只有一个变量实体。我在这里添加了一个没有任何内容的新文件，只包含了头文件 `debug.h`，得到的 `dummy` 数量将会变为30个，多的这一个正是新文件中 `debug.h` 引入的。

>   修改添加的代码, 为两处`dummy`变量进行初始化:`volatile static int dummy = 0;` 然后重新编译NEMU. 你发现了什么问题? 为什么之前没有出现这样的问题? 

出现了报错，不过有意思的是 `common.h` 相关的是报错 error: redefinition，而 `debug.h` 相关的则是 note: previous definition

```error
In file included from /home/nocturne.linux/Project/XM_ICS2024PA/nemu/include/cpu/cpu.h:19,
                 from src/engine/interpreter/init.c:16:
/home/nocturne.linux/Project/XM_ICS2024PA/nemu/include/common.h:49:21: error: redefinition of ‘dummy’
   49 | volatile static int dummy = 0;
      |                     ^~~~~
      
In file included from /home/nocturne.linux/Project/XM_ICS2024PA/nemu/include/common.h:47,
                 from /home/nocturne.linux/Project/XM_ICS2024PA/nemu/include/utils.h:19,
                 from src/engine/interpreter/hostcall.c:16:
/home/nocturne.linux/Project/XM_ICS2024PA/nemu/include/debug.h:43:21: note: previous definition of ‘dummy’ with type ‘int’
   43 | volatile static int dummy = 0;
      |                     ^~~~~
```

这里涉及到的 [Weak Symbols vs Strong Symbols in C](https://embeddedwala.com/Blogs/embeddedsystem/weak-symbols-vs-strong-symbols)，[弱符号](https://en.wikipedia.org/wiki/Weak_symbol)与强符号（弱定义与强定义）。因为符号主要用于链接器进行符号地址的链接，二者分别对应了链接器的不同处理方式。弱符号允许对同一符号的多次定义，不会导致链接错误，可以被覆盖或重写。而强符号则是良定义的实体，在这里是具有唯一地址的变量。C语言中，如果只是声明则是弱定义，即使重复定义也不会影响链接器；而赋值语句则是强定义的，重复定义则会使链接器不知道这个符号该链接哪个地址，导致报错。

## Makefile

>   分析在`am-kernels/kernels/hello/`目录下敲入`make ARCH=$ISA-nemu` 后，`make`程序如何组织.c和.h文件，最终生成可执行文件 `/build/hello-$ISA-nemu.elf`。

因为 hello 的 Makefile 中最重要的就是包含了 `$AM_HOME/Makefile` 文件，因此重点还是 AM 的 Makefile 文件。我们首先分析一下这个程序的构建行为：在目录下输入

```bash
make -np
```

会看到许多注释和编译信息，这些信息可以很好地帮助我们理解：前几条是构建 hello 程序的镜像文件。我们提取出比较重要的编译和链接指令，可以看到都是交叉编译包提供的工具。

在编译时，除了一些编译和链接选项，还进行了头文件路径的选定、设置预定义宏、设置体系结构和目标平台，最后和常规的编译一样选定 `hello.c` 文件进行只编译不链接，输出 .o 目标文件。

接下来是链接过程：`-melf64lriscv` 指定生成 RISC-V 64 位 ELF 格式的文件。但因为目标文件实际上是 32 位的（`riscv32`），因此这里使用了不同的架构和 ABI 设置来生成最终的 32 位可执行文件。--defsym` 定义符号 `_pmem_start`  被定义为 `0x80000000， `_entry_offset` 定义为 `0x0`。`-e _start` 设置程序的入口点为 `_start`。这里很重要，一个程序的执行必然是从入口，到中间执行，再找到出口返回的。我们在前面已经见过`_start` 函数了，其定义位于 `am/src/riscv/nemu/start.S` 中，主要是调用`_trm_init`函数（在`platform/nemu/trm.c`中定义）和栈的初始化。输出文件指定为 RISC-V32 架构的 ELF 文件。最后指定了一些链接静态库和目标文件，比如我们在 AM 中实现的 klib 库函数和编译得到的 `hello.o` 文件。

```bash
riscv64-linux-gnu-gcc -std=gnu11 ... -c -o .../hello.o .../hello.c
riscv64-linux-gnu-ld ... --gc-sections -e _start -melf32lriscv -o .../hello-riscv32-nemu.elf --start-group .../hello.o .../am-riscv32-nemu.a .../klib-riscv32-nemu.a --end-group
```

命令是这么输出的，接下来回到 Makefile 文件本身，因为我们已经对 AM 的 Makefile 进行过 [RTFSC](https://github.com/Nocturne228/XM_ICS2024PA/blob/2024/labnotes/PA2.2.md#rtfsc) 了，这里直接截取部分代码，这里通过 hello 文件的 Makefile 定义的 `SRCS` 变量组织需要进行链接的目标文件（也就是 `hello.o`）。`$(DST_DIR)/%.o` 变量定义了编译命令，通过 Makefile 提供的[自动变量](https://nocturne228.github.io/2024/02/15/makefile0/#%E8%87%AA%E5%8A%A8%E5%8F%98%E9%87%8F)组织起需要编译的文件。 `$(IMAGE).elf` 变量定义了链接命令，即如果选项不是 native 的话，就选择前面设置的静态库和目标文件进行链接。

```makefile
### Collect the files to be linked: object files (`.o`) and libraries (`.a`)
OBJS      = $(addprefix $(DST_DIR)/, $(addsuffix .o, $(basename $(SRCS))))
LIBS     := $(sort $(LIBS) am klib) # lazy evaluation ("=") causes infinite recursions
LINKAGE   = $(OBJS) \
  $(addsuffix -$(ARCH).a, $(join \
    $(addsuffix /build/, $(addprefix $(AM_HOME)/, $(LIBS))), \
    $(LIBS) ))
   

### Rule (compile): a single `.c` -> `.o` (gcc)
$(DST_DIR)/%.o: %.c
	@mkdir -p $(dir $@) && echo + CC $<
	@$(CC) -std=gnu11 $(CFLAGS) -c -o $@ $(realpath $<)


### Rule (link): objects (`*.o`) and libraries (`*.a`) -> `IMAGE.elf`, the final ELF binary to be packed into image (ld)
$(IMAGE).elf: $(LINKAGE) $(LDSCRIPTS)
	@echo \# Creating image [$(ARCH)]
	@echo + LD "->" $(IMAGE_REL).elf
ifneq ($(filter $(ARCH),native),)
	@$(CXX) -o $@ -Wl,--whole-archive $(LINKAGE) -Wl,-no-whole-archive $(LDFLAGS_CXX)
else
	@$(LD) $(LDFLAGS) -o $@ --start-group $(LINKAGE) --end-group
endif

### Rule (archive): objects (`*.o`) -> `ARCHIVE.a` (ar)
$(ARCHIVE): $(OBJS)
	@echo + AR "->" $(shell realpath $@ --relative-to .)
	@$(AR) rcs $@ $^
```

Makefile 中与编译链接相关的内容如上。实际上链接器设置的这些变量都可以体现在反汇编代码中，使用交叉编译工具包中的 `riscv64-linux-gnu-objdump` 进行反汇编

```assembly
hello-riscv32-nemu.elf:     file format elf32-littleriscv


Disassembly of section .text:

80000000 <_start>:
80000000:       00000413                li      s0,0
80000004:       00009117                auipc   sp,0x9
80000008:       ffc10113                addi    sp,sp,-4 # 80009000 <_end>
8000000c:       08c000ef                jal     ra,80000098 <_trm_init>

80000010 <main>:
80000010:       fe010113                addi    sp,sp,-32
...
80000040:       04c000ef                jal     ra,8000008c <putch>
...

8000008c <putch>:
8000008c:       a00007b7                lui     a5,0xa0000
80000090:       3ea78c23                sb      a0,1016(a5) # a00003f8 <_end+0x1fff73f8>
80000094:       00008067                ret

80000098 <_trm_init>:
80000098:       ff010113                addi    sp,sp,-16
8000009c:       00000517                auipc   a0,0x0
800000a0:       04850513                addi    a0,a0,72 # 800000e4 <mainargs>
800000a4:       00112623                sw      ra,12(sp)
800000a8:       f69ff0ef                jal     ra,80000010 <main>
800000ac:       00050513                mv      a0,a0
800000b0:       00100073                ebreak
800000b4:       0000006f                j       800000b4 <_trm_init+0x1c>
```

可以看到 `_start` 和 `_trm_init` 都被链接到程序中，这就是函数的入口，其地址也正为 `0x80000000`。

在文档中也已经指明了程序的出口：调用 `halt()` 函数，这个函数将会调用 `nemu_trap`，其定义为：

```c
// abstract-machine/am/src/platform/nemu/include/nemu.h
# define nemu_trap(code) asm volatile("mv a0, %0; ebreak" : :"r"(code))
```

其含义就是把 `code` 放在 `a0` 寄存器，并且执行 `ebreak`。`ebreak` 显然是框架代码提供的 RISC-V 指令，调用过程如下，实际就是设置 NEMU 的一些状态，使其停机。

```c
  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, R(10))); // R(10) is $a0
|
#define NEMUTRAP(thispc, code) set_nemu_state(NEMU_END, thispc, code)
|
void set_nemu_state(int state, vaddr_t pc, int halt_ret) {
  difftest_skip_ref();
  nemu_state.state = state;
  nemu_state.halt_pc = pc;
  nemu_state.halt_ret = halt_ret;
}
```

>   PA2 DONE∎