# 操作系统 大实验 g10 Verification of File Systems 最终报告

							2015011358，陈经基
							2015011347，朱昊


<!-- vim-markdown-toc GFM -->

* [背景介绍](#背景介绍)
* [任务介绍](#任务介绍)
* [掉电安全文件系统介绍](#掉电安全文件系统介绍)
	* [Crash-Refinement的形式化定义](#crash-refinement的形式化定义)
		* [Definition 1](#definition-1)
		* [Definition 2](#definition-2)
		* [Definition 3](#definition-3)
		* [Definition 4](#definition-4)
		* [Definition 5](#definition-5)
		* [Definition 6](#definition-6)
	* [掉电文件系统开发流程](#掉电文件系统开发流程)
* [Yggdrasil代码介绍与分析](#yggdrasil代码介绍与分析)
	* [环境设置](#环境设置)
	* [Yggdrasil代码](#yggdrasil代码)
		* [verify.py](#verifypy)
		* [test_waldisk.py](#test_waldiskpy)
		* [符号执行引擎](#符号执行引擎)
		* [Z3 SMT Solver](#z3-smt-solver)
* [hv6 FS分析与介绍](#hv6-fs分析与介绍)
	* [LLVMPy Emmiter](#llvmpy-emmiter)
		* [PyEmitter](#pyemitter)
			* [Emitter.hh & Emitter.cc](#emitterhh--emittercc)
			* [PyEmitter.hh & PyEmitter.cc](#pyemitterhh--pyemittercc)
			* [PyLLVMEmitter.hh && PyLLVMEmitter.cc](#pyllvmemitterhh--pyllvmemittercc)
* [工作计划](#工作计划)
* [14-16周完成的工作](#14-16周完成的工作)
	* [工作简述](#工作简述)
	* [实验环境配置](#实验环境配置)
		* [seasnake编译器配置](#seasnake编译器配置)
		* [docker的配置与使用](#docker的配置与使用)
	* [Yxv6文件系统从python到C++的移植工作](#yxv6文件系统从python到c的移植工作)
		* [整体移植思路](#整体移植思路)
		* [WALDisk类 (Transaction层)](#waldisk类-transaction层)
			* [简要介绍](#简要介绍)
			* [移植细节](#移植细节)
		* [InodeDisk类, IndirectInodeDisk类 (VirtualTransaction层，Inode层)](#inodedisk类-indirectinodedisk类-virtualtransaction层inode层)
			* [简要介绍](#简要介绍-1)
			* [移植细节](#移植细节-1)
		* [DirImpl类 (File System层)](#dirimpl类-file-system层)
			* [简要介绍](#简要介绍-2)
			* [移植细节](#移植细节-2)
		* [Bitmap类](#bitmap类)
			* [简要介绍](#简要介绍-3)
			* [移植细节](#移植细节-3)
		* [InodePack类](#inodepack类)
			* [简要介绍](#简要介绍-4)
			* [移植细节](#移植细节-4)
		* [Partition类](#partition类)
			* [简要介绍](#简要介绍-5)
			* [移植细节](#移植细节-5)
	* [将移植到C++的Yxv文件系统接入hv6操作系统](#将移植到c的yxv文件系统接入hv6操作系统)
		* [整体思路](#整体思路)
		* [C与C++的混合编译](#c与c的混合编译)
		* [AsyncDisk Layer](#asyncdisk-layer)
		* [Transaction Layer (WALDisk类](#transaction-layer-waldisk类)
		* [其他胶水代码](#其他胶水代码)
	* [Travis CI持续集成开发](#travis-ci持续集成开发)
	* [实验结果展示](#实验结果展示)
		* [hv6运行与验证](#hv6运行与验证)
		* [移植到C++的Yxv6文件系统的验证](#移植到c的yxv6文件系统的验证)
		* [代码量展示](#代码量展示)
	* [实验结论与收获](#实验结论与收获)
	* [致谢](#致谢)
* [参考文献](#参考文献)

<!-- vim-markdown-toc -->

## 背景介绍
文件系统是操作系统的一个不可缺少的组成成分。而硬盘上的数据结构非常复杂，进行验证与复现bug比较困难。在这个工作中，我们讨论如何写出并验证一个crash-safe的文件系统。

之前的文献中已经提及一个crash-safe的文件系统Yggdrasil，然而这个文件系统

## 任务介绍

接下来对本次大实验中所需要完成的实验任务进行简要介绍：

在本次实验中，我们最主要的工作在于对Yggdrasil这一用于验证掉电安全的文件系统的框架的代码与论文分别进行分析与理解，并且对代码进行注释，并且得到分析文档；其次对hv6操作系统的文件系统进行分析与理解，并且得到分析文档；在完成了足够的分析了解之后，将Yggdrasil中验证的使用cython实现的fuse文件系统Yxv6fs移植到使用C实现的hv6文件系统中，并且使用hv6中原先使用的符号执行框架，与Yggdrasil中进行掉电安全性验证的部分连接起来，从而使得hv6操作系统中拥有一个掉电安全的文件系统。

## 掉电安全文件系统介绍

在接下来将对掉电安全的文件系统进行介绍；在我们参考的论文中，使用了crash refinement作为了文件系统掉电安全性的形式化定义；在该形式化定义下，如果要验证某一个文件系统的掉电安全性，首先需要一份本身就应当被认为是安全的规范设置，称为specification，以及在每一个特定的磁盘状态下应当满足的不变量（表示为一阶谓词逻辑定理）；在拥有了specification的情况下，要求在当前文件系统实现在任何出现系统崩溃并且之后正确地执行了回复程序recovery之后的情况下，磁盘所处的状态与必须与某一个specification中执行了同一操作之后到达的状态等价（之所以说是某个状态，是因为认为specification中能够通过操作和系统奔溃的组合能够到达的所有状态都是合法的，因此特定一个specification中到达的状态，经过指定的某一次操作之后，能够到达的状态并不一定只有一个）；如果所有实现能够达到的磁盘状态均与specification能够达到的某状态想等价，则认为这个文件系统的实现是关于这个specification掉电安全的。

### Crash-Refinement的形式化定义

在下文中将对crash-refinement进行形式化定义：

#### Definition 1

在确定掉电安全性之前，首先需要的是所实现的文件系统能够符合规范的要求，因此需要要求在没有发生任何系统奔溃的情况下，文件系统的实现所能够到达的状态应该是specification中所允许的，因此，要求对于implement中的某一个状态s1与其对应的等价的specification中的状态s0，在执行完指定操作f之后，在发生掉电安全的情况下，他们所到达的状态仍然应当是等价的。

因此，形式化地描述在不发生系统奔溃情况下，实现能够符合规范要求的情况如下：

假设，f0是规范中描述的操作，而f1是实现中描述的操作，而他们所对应的系统不变量分别为I0, I1，则认为f1是在不掉电的情况下符合f0的要求（称之为f0与f1 crash-free equivalent）需要满足如下条件：
![def1](def1.png)


#### Definition 2

接下来描述在不具有recovery操作的情况下，在考虑了系统奔溃的可能下的情况下，需要怎样要求才能够认为实现满足规范的要求：由于所有允许的状态都是在规范中执行相应操作能够到达的状态，因此如果在实现中执行某一个操作，并且遭遇或者没有遭遇到系统奔溃的过程，最终到达的状态仍然能够与规范中允许的某一个状态想等价，则认为是实现本身还是符合了规范的要求；
因此，形式化地定义在考虑了系统奔溃的可能性，并且不考虑存在recovery操作的情况下，实现能够符合规范要求的情况如下：

假设，f0是规范中描述的操作，而f1是实现中描述的操作，而他们所对应的系统不变量分别为I0, I1，则认为f1在上述情况下符合f0的要求（称之为f1是f0的一个crash-refinement without recovery）需要满足如下条件：

![def2](def2.png)

#### Definition 3

由于在文件系统中，很有可能因为系统的奔溃，导致磁盘上维护的数据结构出现了不一致的情况，因此不少文件系统会在奔溃之后的其中的时候启动恢复操作，尝试恢复磁盘上文件系统的不一致性，这样的话文件系统的recovery操作使得文件系统的实现要符合规范的要求要更加简单一些，即不需要强硬的要求实现中执行完某一个操作，并且考虑了奔溃的情况下到达的状态一定需要与规范中要求的状态等价就可以了，而是只需要到达的状态在通过恢复操作之后可能到达与规范中的某个状态相等价的状态即可；自然，恢复操作的过程中也可能经历掉电的过程，在这种情况下，掉电这种情况显然不能够影响后续重启的时候执行的恢复操作的正确执行，因此，只要存在一次恢复操作成功了，就能够修复磁盘的不一致状态。

将上述性质称为了恢复操作的幂等性，将其进行形式化定义如下：

假设r是一个恢复函数，则其满足幂等性需要满足以下条件：

![def3](def3.png)

#### Definition 4

接下来讨论在存在恢复操作的情况下，实现能够符合规范的要求需要满足的形式化条件如下所示：

假设f0是规范中定义的某一个操作，而f1是实现中定义的与f0相对应的操作，而I0，I1分别是指这两个操作所需要满足的系统不变量，而r是幂等的恢复操作，而f0,f1本身在不考虑系统奔溃的情况下是等价的，则f1是满足了f0要求的实现（记为f1是f0的一个crash-refinement），则需要还满足的条件有：

![def4](def4.png)

#### Definition 5

由于存在某些操作不会对磁盘状态造成影响，因此对这些操作进行形式化定义如下：

![def5](def5.png)

#### Definition 6

接下来将文件系统F定义为文件系统的操作f的集合，则认为文件系统的实现F1满足了规范F0需要满足F1中所有的操作f1都满足了F0中对应的操作f0的要求；

### 掉电文件系统开发流程

接下来介绍开发掉电安全的文件系统的开发流程：

1. 编写文件系统需要满足的规范；
2. 编写需要满足的条件不变式；
3. 编写文件系统的实现；
4. 验证文件系统的实现满足规范；
5. 对文件系统的实现进行优化；

## Yggdrasil代码介绍与分析

### 环境设置

接下来简要介绍Yggdrasil和hv6的环境配置：

配置Yggdrasil环境：

1.  安装Z3
2.  安装fuse头文件，否则无法通过pkg-config正确获取相应的cflags
3.  创建文件系统的磁盘镜像
4.  创建fuse文件系统的挂载点（设置为a）
5.  编译yggdrasil
6.  挂载文件系统
7.  进行验证

配置hv6环境：

1. 安装qemu;
2. 更新编译工具;
3. 此时已经可以使用qemu运行hv6操作系统了;
4. 由于hv6原先实在ubuntu 17.10上编写的，使用的编译环境版本较高，使用make verify验证的时候有可能无法通过，因此需要对代码进行部分修改;
5. 使用make verify进行验证了;

最后环境配置成docker image，并且存放在docker hub上，分别为amadeuschan/osproject, amadeuschan/osproject_with_hv6，前者适用于yggdrasil中文件系统的验证与运行，而后者适用于两者的验证与运行；

此外，还在travis CI上配置了自动测试，由于travis CI默认需要10min内至少有一次对stdout的显式输出，而在yggdrasil中，某些test需要的时间要略长于10min，因此实现了后台脚本每隔10min往stdout输出的方式来欺骗travis CI平台，从而使得能够正确地完成测试。


### Yggdrasil代码

#### verify.py

入口程序，通过shell调用

1. ('test_waldisk.py', 'WAL Layer'),

2. ('test_xv6inode.py ', 'Inode layer'),

3. ('test_dirspec.py', 'Directory layer'),

4. ('test_bitmap.py', 'Bitmap disk refinement'),

5. ('test_inodepack.py', 'Inode disk refinement'),

6. ('test_partition.py', 'Multi disk partition refinement')

六个测试程序。

#### test_waldisk.py

这个程序是用来测试WAL Layer的。总体上，使用python的unittest框架来自动执行以test开头的两个程序。

test_idempotent_recovery函数：测试Def 3:r(f(x, b), true) = r(x, true)。这个函数首先创建了一个Machine对象，这个类是在yggdrasil/diskspec.py中定义的。这个函数主要分为四步

对恢复后的情况的检验 得到未crash恢复之后的磁盘状态x 得到crash恢复之后的磁盘状态y 检验x == y

test_atomic函数：证明Def 6，也即crash之后再恢复的状态为crash之前的状态或者已经执行过crash时的操作的状态。

#### 符号执行引擎

#### Z3 SMT Solver

## hv6 FS分析与介绍

1. 该文件系统共分为7层，从下向上分别为：
   1. Disk
   2. Buffer cache
   3. logging
   4. inode
   5. directory
   6. pathname
   7. file descriptor

其中各层向上层提供的服务主要为：
   1. Disk: iderw，提供了往物理磁盘的指定扇区读写数据的功能（根据后文中xv6的实现，猜测该接口实现的服务为原子操作）；
   2. buffer cache: 维护了一系列物理磁盘上的某些扇区在内存里的备份，其中每一个扇区只能有一个备份，每一个备份只能由一个进程占有；具体提供的接口有：
      1. pread: 返回指定扇区在内存中的备份；
      2. pwrite: 将对内存中磁盘扇区备份的修改写入物理磁盘；
      3. prelse: 当前进程释放对当前持有的某个buffer的占用；
   3. logging: logging层为文件系统提供了具有原子性的写若干个block的服务，称为transaction，其实现方式为，在磁盘上的某一特定区域维护一个log分区，其中log分区包括了分区头以及若干数量的用于存储要写入内容的block；具体实现原子性的做法为：在要进行对若干个block的写操作的时候，将这些需要一起进行的写操作分别写入到log分区中，在完成了对log分区的写操作之后，将分区头中需要写的block的数量置为非零；之后再进行对磁盘对应区域的写操作；如果当前文件系统crash了，在重启之后，执行recovery操作，首先会检查log分区头中是否需要写的block数量为零，如果不是则完成log分区中记录的所有写操作；一旦完成了所有写操作，再将分区头的计数置为0；这样的话就可以保证原子性了：如果在写入log分区，还没有修改计数为非零的时候crash，recovery的时候则完全不会考虑到这若干写操作，则这个transaction完全不会发生；而如果在写入log分区之后，还没有完成对磁盘的所有写操作之前crash，恢复操作会考虑到log中还有没有完成的写操作，会重新进行整个transaction的写操作；如果在完成所有写操作并且修改了分区头的计数为0之后，则已经完成了整个transaction的写操作；综上，可以发现，只要保证对某一个block的写的原子性，则可以保证整个transaction的写的原子性；具体本层提供的接口有：
       1. begin_op: 开始一个transaction;
       2. log_write: 往log中写入对某一个block的写操作；
       3. write_head: 修改log分区头；
       4. install_trans: 将log分区中的所有修改写入磁盘；
       5. end_op: 结束一个transaction；
       6. recover_from_log: 在crash之后进行恢复操作；
   4. inode: 提供了对磁盘上与内存中的inode一系列管理接口，包括对指定inode的第指定个数据块的读取和写入操作，使用上一层的transaction保证操作的原子性；
   5. directory: 认为目录是一种特殊的文件，其数据块中存储了一系列该目录下的文件名和inode信息；
   6. pathname: 使用文件路径名寻找到对应innode；
   7. file descriptor：便于将其他接口（dev等）抽象成文件；



### LLVMPy Emmiter
#### PyEmitter
##### Emitter.hh & Emitter.cc

* The Emitter class is in `namespace irpy`
* Member: an ostream and an indent level
* Only constructed by calling Emitter(stream = stream, indent_level = 0)
* Two methods: 
    1. line(string) indents for indent_level times and output the string
    2. line(void) output a return symbol

##### PyEmitter.hh & PyEmitter.cc

* PyEmitter is a subclass of Emitter
* Constructed by calling PyEmitter(stream)
* Four methods:
    1. genBlock(string, function<void()>) output the block + ":"; indent; execute the function; unindent
    2. genDef(string, vector<string>, function<void()>) output "def " + function's name + "(" + args' names + ")" + ":"; indent; excute function; unindent
    3. genException(string) output an Exception message
    4. emitWarning(string) give warning that this file is automatically generate from another file.

##### PyLLVMEmitter.hh && PyLLVMEmitter.cc

* PyLLVMEmitter is a subclass of PyEmitter
* Constructed by calling PyLLVMEmitter(stream, module) module is an instance of LLVM:Module
* Six methods:
    1. emitModule(void) 
    2. emitMetadata(void);
    3. emitBasicBlock(llvm::BasicBlock &bb);
    4. emitStructType(const llvm::StructType &type);
    5. emitFunction(llvm::Function &func);
    6. emitGlobalVariable(const llvm::GlobalVariable &type);
* function quoto(string) add quotes to a string
* function nameType(llvm::Value) return "itypes.parse_type(ctx," + the Value's type + ")"
* function getPrintingName(llvm::Value, bool, llvm::Module) return the name of this llvm::Value
* MetadataVisitor is a subclass of llvm::InstVisitor<MetadataVisitor>
* Constructed by MetadataVisitor(llvm::Module, bool) the Module to visit and recursive or not
* Four methods:
    1. addMDNode(llvm::MDNode) add this MDNode to the set of meta data nodes; if recursive also add its operands
    2. visitFunction(llvm::Function) add all metadata attached to the function
    3. visitInstruction(llvm::Instruction) add all metadata attached to the instruction
    4. getMetaData(void) get the list of identifier, metadata pairs
* PyInstVisitor is a subclass of llvm::InstVisitor<PyInstVisitor>
* Constructed by PyInstVisitor(PyEmitter, llvm::Module) 
* functions:
    1. genPyCallFromInstruction(bool, string, T, kwargs_t)
    2. genPyCall(string, args_t, kwargs_t) together with _genPyCall(stringm, args_t, kwargs_t) generate a function call "irpy."+string(ctx, args, kwargs)
    3. name(llvm::Value) get the name of Value
    4. get(llvm::Value) if Value is an instruction, it returns ctx.stack["name(i)"]; if Value is a constant, it returns visitConstant(Value); if Value is am Argument, it returns ctx.stack["name(i)"]; if Value is an InlineAsm, it returns python function call asm + asmstring with quote.

## 工作计划 
1. 将hv6FS的代码做irpy产生符号化执行图
2. 将1.中的执行图在Yggdrasil中做验证（难点）
3. 将Yggdrasil的FS port到hv6中（难点）
4. 验证新写出的FS代码

## 14-16周完成的工作

### 工作简述

### 实验环境配置

#### seasnake编译器配置

#### docker的配置与使用

### Yxv6文件系统从python到C++的移植工作

#### 整体移植思路

#### WALDisk类 (Transaction层)

##### 简要介绍

##### 移植细节

#### InodeDisk类, IndirectInodeDisk类 (VirtualTransaction层，Inode层)

##### 简要介绍

##### 移植细节

#### DirImpl类 (File System层)

##### 简要介绍

##### 移植细节

#### Bitmap类

##### 简要介绍

##### 移植细节

#### InodePack类

##### 简要介绍

##### 移植细节

#### Partition类

##### 简要介绍

##### 移植细节

### 将移植到C++的Yxv文件系统接入hv6操作系统

#### 整体思路

#### C与C++的混合编译

#### AsyncDisk Layer

#### Transaction Layer (WALDisk类

#### 其他胶水代码

### Travis CI持续集成开发

### 实验结果展示

#### hv6运行与验证

#### 移植到C++的Yxv6文件系统的验证

#### 代码量展示

### 实验结论与收获

### 致谢

## 参考文献

* Sigurbjarnarson, Helgi, et al. "Push-Button Verification of File Systems via Crash Refinement." OSDI. Vol. 16. 2016.
* Nelson, Luke, et al. "Hyperkernel: Push-Button Verification of an OS Kernel." Proceedings of the 26th Symposium on Operating Systems Principles. ACM, 2017.
* Joshi, Rajeev, and Gerard J. Holzmann. "A mini challenge: Build a verifiable filesystem." In Verified Software: Theories, Tools, Experiments, pp. 49-56. Springer, Berlin, Heidelberg, 2008.
* Chen, Haogang, Tej Chajed, Alex Konradi, Stephanie Wang, Atalay İleri, Adam Chlipala, M. Frans Kaashoek, and Nickolai Zeldovich. "Verifying a high-performance crash-safe file system using a tree specification." In Proceedings of the 26th Symposium on Operating Systems Principles, pp. 270-286. ACM, 2017.
* T. S. Pillai, V. Chidambaram, R. Alagappan, S. AlKiswany, A. C. Arpaci-Dusseau, and R. H. ArpaciDusseau. All file systems are not created equal: On the complexity of crafting crash-consistent applications. In Proceedings of the 11th Symposium on Operating Systems Design and Implementation (OSDI), pages 433–448, Broomfield, CO, Oct. 2014.
