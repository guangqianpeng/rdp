## RDP - A Reliable ARQ Protocol Based on UDP

### 简介

RDP是一个基于UDP的可靠传输协议，其内部为纯算法实现，用户需要传入output callback和timer才能使用。RDP小巧（~1200 loc）但具备了TCP的关键特性，例如拥塞控制，超时重传等。

### 使用

NetCat是我用RDP写的一个例子，类似于Linux下的nc，可以用来传输文件或者双向通信。

### 性能

测试结果显示RDP的吞吐量约为TCP的60%~%70，目前正在想办法提升。