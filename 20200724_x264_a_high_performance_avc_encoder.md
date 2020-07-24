# 1. 引言

H.264/AVC 的官方代码 JM 因为速度问题，使用受限。x264 应用广泛，性能甚至超过许多商业编码器，而且用到了很多开源软件如 ffdshow, ffmpeg, MEncoder 中。

# 2. X264

X264 的高性能主要归功于它的 Rate Control、Motion Estimation、MacroBlock Mode Decision、Quantization 和 Frame Type Decision 等等一系列算法。除此之外，X264 还在许多基础操作上使用了汇编优化。

## 2.1 Rate Control

在指定比特率和视频缓冲区的约束条件下，Rate Control 会选择合适的编码参数获取最高的视频质量。H264 中的 Rate Control 会在三种不同的级别执行：视频组(GOP)、帧级别和宏块级别。在每个级别上，Rate Control 选择 QP 的值，它决定转换系数的量化。增加 QP 的值，会增大量化步长，降低比特率。X264 中的 Rate Control 算法是基于 libavcodec 实现的，主要是经验性的。X264 中一共有五中 Rate Control 方法。在假设参考解码器(VBV)模式下，X264 允许每个宏块有不同的 QP 值，其他模式下，QP 用于整个帧。

### 2.1.1 Two Pass(2 pass)

使用该方过程中，在第一遍中，获取每一帧的数据，从而可以使得 X264 可以在文件中全局分配 bits。一次编码后，接下来的二次编码实现步骤如下：

1. 首先，两个 P 帧之间分配的相关 bits 数是与总的 bit 数相独立的。它的计算公式为`bits = complexity^0.6`。其中 complexity 是给定的帧在某个固定 QP 条件下，所预测的 bit 大小。I 帧和 B 帧不会再单独做分析，而是使用与之临近的 P 帧的 QP。
2. 缩放上面 1 的结果来填充所需文件的大小。
3. 编码在该步进行。编码完每一帧后，更新后面的 QP 来修正预测大小有误的情况。如果二次编码始终与预测大小不符，就将`offset = 2^((real filesize/predicted filesize)/6)` 叠加到后面的 QP 上。除了`long-term compensation`，还有`short-term compensation`来防止 X264 在最开始或最后面的所需文件大小偏离过大。`short-term compensation`是基于编码大小和目标大小的绝对值，而不是比值。

### 2.1.2 Average Bitrate(ABR)

`one pass`的编码方案必须在不知道未来帧信息的情况下，完成码率控制(Rate Control)。因此，ABR 模式无法准确的得到目标文件大小。接下来的步骤，与 2pass 中的步骤匹配起来。

1. 与 2pass 相似，只是它的 Estimating Complexity 不再来自之前的编码，而是对帧的一半儿分辨率进行快速的 Motion Estimation 算法，并且使用残差的 SATD 作为复杂度。同时，接下来的 GOP 的复杂度也是未知的，因此 I 帧的 QP 是基于以前的帧。
2. 因为不知道未来帧的复杂度，因此只能基于之前的信息做缩放。缩放因子选择那个能够达到指定分辨率的。如果它应用于所有的帧。
3. long term和 short term 压缩与 2pass 中的相同。通过调节压缩强度，可以获得与 2pass 接近的质量，只是文件大小上下有 10% 的误差；或者较低的质量但在严格文件大小控制。

### 2.1.3 VBV-compliant constant bitrate(CBR)

该模式是为实时码流设计的`one-pass`模式。

1. 它使用与 ABR 相同的 complexity estimation 来计算 bit 大小。
2. 用于计算文件大小的缩放因子(scaling factor)是基于 VBV Buffer Size 而不是过去帧计算得来的。
3. OverFlow Compensation 使用与 ABR 中相同的算法，但是它会在每行宏块结束后运行，而不是每帧结束后运行。

### 2.1.4 Constant Rate-Factor

这也是一种 one pass 模式，如果你只希望获得特定的质量，而不是特定的比特率，这是最佳的选择。除了缩放因子(scaling factor)是由用户指定以及没有 OverFlow Compensation 外，它与 ABR 相同。

### 2.1.5 Constant Quantizer

这也是一种 one pass 模式，其中 QP 简单的基于帧的类型是 I 、P 或 B 帧。
