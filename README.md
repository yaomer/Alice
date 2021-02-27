```
         ___
  ____ _/ (_)_______
 / __ `/ / / ___/ _ \
/ /_/ / / / /__/  __/
\__,_/_/_/\___/\___/

```
### 功能：
+ 支持字符串、列表、哈希、集合、有序集合
+ 过期键
+ 持久化
+ 主从复制
+ sentinel
+ proxy
+ 事务
+ 发布订阅
+ 排序
+ 慢查询日志
+ 内存淘汰

### 存储引擎
可在配置文件中根据需要选用下面两种存储引擎：
+ mmdb：类似于redis，基于内存，可持久化
+ ssdb：基于leveldb，数据落盘

### 依赖
+ 网络模块使用[Angel](https://github.com/yaomer/Angel)
+ 客户端的命令提示使用[linenoise](https://github.com/antirez/linenoise)
+ ssdb底层需要使用[leveldb](https://github.com/google/leveldb)，leveldb编译时需要`-frtti`选项
+ mmdb-rdb和ssdb-leveldb需要使用`snappy`
+ 我们默认libangel.a liblinenoise.a libsnappy.a都安装到了/usr/local/lib目录

### 压测
+ 压测可使用[redis](https://github.com/antirez/redis)自带的benchmark(有一点需要注意的是：必须指定-t cmd)

#### 与redis的测试对比
+ 单线程
![](https://github.com/yaomer/pictures/blob/master/alice_bench.png?raw=true)

+ 多线程
![](https://github.com/yaomer/pictures/blob/master/alice_bench1.png?raw=true)

+ 功能性测试
![](https://github.com/yaomer/pictures/blob/master/redis-bench-all.png?raw=true)
![](https://github.com/yaomer/pictures/blob/master/alice-bench-all.png?raw=true)

### 和Alice一起玩
![](https://github.com/yaomer/pictures/blob/master/alice_play.png?raw=true)

### 用户API
我们提供了一个简单的同步API，使用方法见[这里](https://github.com/yaomer/Alice/blob/master/client/example/tmp.cc)
