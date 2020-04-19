# 一个用于H264视频编码的新颖的高性能优化算法

## 5. MacroBlock-Tree Algorithm (MB-Tree 算法)

对于每一帧，我们在所有宏块上执行 propagate step，MacroBlock-Tree 操作的 propagate step 如下：

1. 对于当前宏块，读取下面变量的值：
* intra_cost: 该宏块的帧内模式的预测 SATD 代价。
* inter_cost: 该宏块的帧间模式的预测 SATD 代价。如果该值比 intra_cost 大，设置其为 intra_cost。
* propatate_in: 该宏块的 propagate_cost。因为没有任何信息，执行 propagate 的第一帧，它的 propagate_cost 值为 0。
2. 计算要执行 propagate 的当前宏块对其参考帧的宏块的信息的分数，称为 propagate_fraction。计算方法为 1 - intra_cost / inter_cost。例如，如果 inter_cost 是 intra_cost 的 80%，我们说该宏块有 20% 的信息来自于它的参考帧。
3. 和当前宏块有关的所有信息总和大约为 intra_cost + propagate_cost（自身信息和提供给后续帧的信息），使用这个总和乘以继承率 propagate fraction, 可以得到来继承自参考帧的信息量 propagate amount。
4. 将 propagate_amount 划分给参考帧中相关的宏块，由于当前宏块在参考帧中运动搜索得到的补偿区域可能涉及多个宏块，即参考帧中的多个宏块都参与了当前宏块的运动补偿，所以我们根据参考帧宏块参与补偿的部分尺寸大小来分配 propagate amount。特别的，对于 B 帧，我们把 propatate amount 先平分给两个参考帧，再进一步分配给参考帧中的宏块。参考帧中的宏块最终被分到的 propagate amount 加起来就是它的 propagate cost。
5. 从前向预测的最后一帧向前一直计算到当前帧，可以得到当前帧中每个宏块对后续 n 帧的 propagate_cost，最后根据当前帧每个宏块的 propatate_cost，计算相应的偏移系数 qp_offset，所使用的公式如下：

MacroblockQP = -strength * log2((intra_cost + propagate_cost) / intra_cost)。

其中强度系数 strength 为常量，对于未被参考的宏块而言，propagate_cost = 0, qp_offset = 0。

X264 源码中实现MB-Tree 的函数为 macroblock_tree，其中调用了如下三个函数来实现上述步骤：

1. slicetype_frame_cost():计算宏块的帧内代价和帧间代价。
2. macroblock_tree_propagate():计算当前宏块的遗传代价。
3. macroblock_tree_finish():计算量化参数偏移系数。

X264 中的调用关系如下：
x264_slicetype_analyse  
  ->macrblock_tree  
    ->slicetype_frame_cost  
      ->slicetype_slice_cost  
        ->slicetype_mb_cost  
    ->macroblock_tree_propagate  
    ->macroblock_tree_finish  

