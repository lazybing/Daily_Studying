# [frame_num is not frame counter](http://www.ramugedia.com/frame-num-is-not-frame-counter)

谨记，H264 的 slice header 中的 frame_num 参数并不是与 frame counter 完全相同，因为 frame_num 在非参考帧后的帧数并不会增加。因此，如果视频序列中的所有帧都是参考帧，则 frame_num = frame counter。

例如下列序列中，下角标代表 frame_num 的值，上角标代表 POC 的值，大写字母对应的帧是参考帧：

I~0~^0^ P~1~^6^ b~2~^2^ b~2~^4^ P~2~^8^ P~3~^10^  

这里的b~2~^2^ b~2~^4^ 是一对 B 帧，用作非参考帧。

## 优势1：连续的 B 帧可以交换位置

在上面的序列中，假设为了保证 HRD 的合规性，需要交换 b~2~^2^ 和 b~2~^4^（比如，假定 b~2~^4^ 币平均的 B 帧大小要大很多）。因为连续的两个 B 帧的 frame_num 值相等，两帧是独立的，因此交换两帧并不会影响解码，但可能会防止 HRD 违规。因此编码器可以传输上面的码流序列按照如下顺序：  

I~0~^0^ P~1~^6^ b~2~^4^ b~2~^2^ P~2~^8^ P~3~^10^ 

如果 frame_num 按照 frame counter 的顺序增长，就不能交换非参考的 B 帧。

## 优势2：流瘦身-去掉非参考的 B 帧

有时由于 bandwidth 的问题，服务器会通过去掉非参考的 B 帧来瘦身传输的流。因为非参考帧并不会用于后续帧的参考，去掉非参考帧不会影响后续图像。因此去掉非参考的 B 帧必然会降低码率（尽管可能会出现视频抖动）。而且因为 frame_num 在非参考帧时不会增加，因此也不会出现 frame_num 跳跃的现象。

如果 frame_num 与 frame counter 一样递增，就不可能出现去掉非参考帧的情况下 frame_num 不跳跃的情况。

## 优势3：帧率校正-重复帧

有时可能需要帧率校正，比如从低帧率到高帧率(如从 30fps 到 60fps)，此时某些帧会重复。

简单起见，假设有一个只有 IP 帧的码流，需要将其帧率从 29.97fps 提升至 30fps。可以不重新编码该码流的前提下，每 300 帧插入一个非参考的 P-skip 帧。该 P-skip 帧是一个所有 MB 都被 skipped 的 P 帧（所有的运动矢量都为 0）。因为 P-skip 帧并不用做参考，它的 frame_num 不会增加，不会出现 frame_num 的跳跃。

注意 B 帧同样可以作为参考帧。

# [Picture Order Count](https://www.vcodex.com/h264avc-picture-management/)

先给出 SPEC 中关于 POC 相关的语法定义，主要包括两部分，一部分是在 SPS 中定义的，一部分是在 Slice Header 中定义的。

```
//part one: SPS Data
pic_order_cnt_type
if (pic_order_cnt_type == 0)
    log2_max_pic_order_cnt_lsb_minus4
else if (pic_order_cnt_type == 1) {
    delta_pic_order_always_zero_flag
    offset_for_non_ref_pic
    offset_for_top_to_bottom_field
    num_ref_frames_in_pic_order_cnt_cycle
    for( i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
        offset_for_ref_frame[ i ]
}
//part two: SliceHeader Data
if (pic_order_cnt_type == 0) {
    pic_order_cnt_lsb
    if( bottom_field_pic_order_in_frame_present_flag && !field_pic_flag )
        delta_pic_order_cnt_bottom
}
if (pic_order_cnt_type == 1 && !delta_pic_order_always_zero_flag) {
    delta_pic_order_cnt[ 0 ]
    if( bottom_field_pic_order_in_frame_present_flag && !field_pic_flag )
        delta_pic_order_cnt[ 1 ]
}
```

SPS 中决定该段码流使用的 POC 类型，根据类型不同，POC 的计算有 3 种方法：  
* 类型0：在每个 slice header 中，直接显示的传送 POC。
* 类型1：在 SPS 中设定预期的增量，如果预期的 POC 有改定，只传送 delta 值。
* 类型2：显示顺序与解码顺序相同，不需要传送额外多余的字符。

## POC_Type 0
每个 Slice Header 中有 PrevPOCLsb，通过 PrevPOCLsb 计算最终的 POC。
1. 计算 PrevPOCMsb 和 PrevPOCLsb
2. 计算 POCMsb
3. POC = POCMsb + POCLsb

## POC_Type 1
每个 Slice Header 中有 Delta POC，通过 Delta POC 计算最终的 POC。
1. 计算 FrameNumOffset、AbsFrameNum、ExpectedDeltaPOCCycle、ExpectedPOC。
2. POC = ExpectedPOC + Delta_POC.

## POC_Type 2
无需传送额外的信息，通过 frame_num 计算最终的 POC。