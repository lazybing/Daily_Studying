# [参考帧列表](https://www.vcodex.com/h264avc-picture-management/)

解码出来的参考图像通常放在 DPB Buffer 中，并标记为一下两种之一：

* short term reference picture, 根据 frame_num 或 POC 来确定下标
* long term reference picture, 根据 LongTermPicNum 来确定下标，该参考编号是从 LongTermFrameIdx 参数获得的，是在图像标记为长参考帧时分配的。

short term reference picture 同样可能被分配一个 LongTermFrameIdx，稍后会被改为 long term reference picture。

short term reference picture 有两种情况可能会从 DPB Buffer 中移除：
1. 码流中有明确的标记指出移除 short term reference picture。 
2. DPB 已满并且 DPB的处理模式是自动模式时，最早的 short term reference picture 会被移除。

long term reference picture 只有码流中有明确标记时才会从 DPB 中移除。

## 默认参考帧列表

在编码或解码一个 Slice 前，参考帧会在参考帧列表中优先排好序，用于后续的编解码。P Slice 只使用一个参考帧列表 list0；B Slice 同时使用两个参考帧列表 list0 和 list1。每个列表中，short term reference picture 在前，long term reference picture 在后，按照 LongTermPicNum 递增的顺序排列。如果当前的 Slice 是 P Slice时，默认的 short term reference picture 顺序依据解码顺序；当前的 Slice 是 B Slice 时，默认的顺序依据现实顺序。

* List0(P Slice):按照 PicNum 递减的顺序作为默认顺序，PicNum 是另外一种形式的 frame_num，与 maxframenum 取余得到。
* List0(B Slice):参考帧的 POC 比当前帧的 POC 小时，按照递减顺序排列，参考帧的 POC 比当前帧的 POC 大时，按照递增顺序排列。
* List1(B Slice):参考帧的 POC 比当前帧的 POC 小时，按照递减顺序排列，参考帧的 POC 比当前帧的 POC 大时，按照递增顺序排列。

## 重排参考帧列表

对于 B Slice，在解析 slice header 时，参考列表 list0 和 list1 的默认顺序可能会有调整，该调整是由语法元素 ref_pic_list_reordering_flag 触发的，并且仅对当前 slice 有效。例如，该方法可能用在使能编码器将一个特殊的参考帧放到列表中的更前面，因为将参考帧放到前面可以节省码率。

1. 初始化指针(refIdxL0)，指向参考帧列表 list0 的第一个参考帧。
2. 当 reordering_of_pic_nums_idc != 3 时
    * 选择一个参考帧，如果是 short term 的，就选择 abs_diff_pic_num_minus1 标记的，如果是 long term 的，就选择 long_term_pic_num 标记的那个。
    * 将所有其他参考帧从列表中的 refIdxL0 位置向前移动一个位置
    * 将该参考帧放到列表中 refIdxL0 指示的位置。
    * 增加指针 refIdxL0
