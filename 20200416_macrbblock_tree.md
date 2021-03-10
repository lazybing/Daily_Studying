# 一个用于H264视频编码的新颖的高性能优化算法

## 1. 概述

帧序列中比特位的智能分配对于获得高压缩率是至关重要的。优化该问题的标准方法是R-D曲线优化。然而在多个独立的帧序列之间，找到比特分配R-D方案通常是不可行的，因为这需要复杂的计算。

大多数现存的方案，不管是启发式的还是最优的，都有不切实际的高复杂性，或者只能提供非常小的压缩提升。还有，尽管他们都有很高的复杂度，现存方案仍然只是假定每一帧有同样的量化参数。我们提出了一个新颖的宏块树算法来优化多个相互依赖的帧之间每个块的量化参数，该方法增加的计算量可以忽略不计。

该论文组织如下：第二部分提出了R-D优化比特率问题的背景以及存在的启发式算法。第三部分给出了MB-tree算法的概述预览以及目的。第四部分x264中的lookahead架构，它是我们能够实现MB-tree算法的基础。第五部分介绍了MB-tree算法本身。第六部分包含了该算法的一系列分析。第七部分讨论了MB-tree相关的感性考量。第八部分给出数值质量结果。第九部分是性能分析。第十部分是结论。

## 3. Macroblock-Tree 的高层概述

MB-tree 算法的目的是预测信息量，该信息量表示每个宏块对未来帧的贡献。该信息允许MB-tree基于其贡献，加权每个树的质量宏块。为此，MB-tree的工作方向与预测方向相反，将信息从将来的帧传播回要编码的当前帧。

为此，MB-tree 需要知道多种信息，或者至少近似的信息量。首先，它必须知道即将分析的未来帧的帧类型。其次，它必须知道这些帧的运动向量。第三，它必须知道每个步骤要传播多少信息量，这会根据帧内和帧间消耗来计算。接下来描述的lookahead会说明如何获取这些信息。

## 4. x264 lookahead

x264 有个复杂的lookahead模块，该模块设计用来，在真正的编码模块分析之前，预测帧的编码消耗。它用这些预测信息来做很多的决定，比如自适应的B帧的位置、显示加权预测、以及缓冲区受阻的码率控制的比特分配。因为性能的原因，它的操作是对一半分辨率进行的，并且仅仅计算SATD残差，并不做量化和重建。

lookahead的核心是`x264_slicetype_frame_cost`函数，它会被重复的调用来计算p0/p1/b的帧代价。p0是被分析帧的前向预测帧，p1是被分析帧的后向预测帧，b是被分析的帧。如果p1
等于b，则分析的帧是P帧。如果p0等于b，则分析的帧是I帧。因为`x264_slicetype_frame_cost`可能会在算法中被重复调用很多次，每次调用的结果都要保存下来以备未来使用。

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

