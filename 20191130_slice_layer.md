## Slice Layer

### Slice Types

每个编码的图像、帧或者场，都是由一个或多个 Slice 组成的，每个 Slice 又包含一个 Slice Header 和多个宏块 MarcroBlock。每个 Slice 中宏块的个数不固定。每个编码的 Slice 之间数据没有依赖关系，这样可以降低错误在不同的片之间进行传播。选择 Slice 大小的方案有如下几种：

* 每个 Slice 就是一张完整的图片，这在很多 H264 编码应用中是很常见的。
* 每张图包含 N 个 Slice，每个 Slice 包含 M 个宏块（M、N都是整数），此种情况，每个编码的 Slice 中包含的字节数并不固定，而是随着图像的内容而改变。
* 没张图包含 N 个 Slice，每个 Slice 包含不固定的宏块数，此种情况，会使得每个编码的 Slice 大小大致相同。这种编码方案对于固定大小的网络包的情况是很有用的。

下表给出了每个 Slice 中可能的 Slice 类型和宏块类型。注意，B Slice 中可能有 B、P 或 I 宏块类型，编码器会根据序列的内容选择合适的宏块类型。

| Slice Type | Containes macroblock types | Notes |
| :-----: | :----: | :-----: |
| I(including IDR) | I only | Intra prediction only |
| P | I and/or P | Intra prediction (I) and/or prediction from one reference per macroblock partition |
| B | I, P and/or B | Intra prediction (I), prediction from one reference (P) or biprediction prediction from two references (B) |
| SP | P and/or I | Switching P slice |
| SI | SI | Switching I Slice |

## Slice Header

Slice Header 中包含了 Slice 中的每个宏块传递的信息，例如 Slice Type 就决定了该 Slice 中每个宏块的可能类型、该 Slice 属于的 frame_num 数、参考帧的设置和默认的量化参数。  

下表给出了一个从 I Slice 中的 Slice Header 的示例，并且 frame_mbs_only_flag 参数为 1， 表示没有场图，它是第一个 I slice，因此同时是一个 IDR slice。默认的 QP 设置为初始值为 26 + 4。  

| Parameter | Binary code | Symbol | Discussion |
| :----: | :----: | :----: | :----: |
| first_mb_in_slice | 1 | 0 | First MB is at position 0, the top-left position in the slice |
| slice_type | 1000 | 7 | I slice, containes only I MBs |
| pic_parameter_set_id | 1 | 0 | Use PPS 0 |
| frame_num | 0 | 0 | Slice is in frame 0 |
| idr_pic_id | 1 | 0 | IDR#0: only present in IDR picture |
| pic_order_cnt_lsb | 0 | 0 | Picture order count = 0 |
| no_output_of_prior_pics_flag | 0 | 0 | Not used |
| long_term_reference_flag | 0 | 0 | Not used |
| slice_qp_delta | 1000 | 4 | QP = initial QP + 4 = 30 |

## Slice Data

Slice Data 是由多个宏块组成的，在编码中，很多宏块不含任何数据，即 skip 宏块。它由 mb_skip_run 参数传递，该参数使用 CAVLC 编码，或者 mb_skip_skip 参数传递，改参数使用 CABAC 编码。skip MB 只会发生在 P、SP 或 B Slice中。一个完整的 Slice Data 包含 coded 和 skipped 宏块。