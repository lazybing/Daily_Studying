# 1. 引言

H.264/AVC 的官方代码 JM 因为速度问题，使用受限。x264 应用广泛，性能甚至超过许多商业编码器，而且用到了很多开源软件如 ffdshow, ffmpeg, MEncoder 中。

# 2. X264

X264 的高性能主要归功于它的 Rate Control、Motion Estimation、MacroBlock Mode Decision、Quantization 和 Frame Type Decision 等等一系列算法。除此之外，X264 还在许多基础操作上使用了汇编优化。

## 2.1 Rate Control

在指定比特率和视频缓冲区的约束条件下，Rate Control 会选择合适的编码参数获取最高的视频质量。H264 中的 Rate Control 会在三种不同的级别执行：视频组(GOP)、帧级别和宏块级别。在每个级别上，Rate Control 选择 QP 的值，它决定转换系数的量化。增加 QP 的值，会增大量化步长，降低比特率。X264 中的 Rate Control 算法是基于 libavcodec 实现的，主要是经验性的。X264 中一共有五中 Rate Control 方法。在假设参考解码器(VBV)模式下，X264 允许每个宏块有不同的 QP 值，其他模式下，QP 用于整个帧。

### 2.1.1 Two Pass(2 pass)

