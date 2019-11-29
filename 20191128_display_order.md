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
