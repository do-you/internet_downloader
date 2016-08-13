internet_downloader
===========================
b/s模式的http下载器,前端为YAAW.目前v3只是个demo  

Design goal
------------
* **读写分离**:即使在极高速情况下,写硬盘也不会拖慢下载速度
* **兼容aria2**:符合aria2的json-rpc格式,目前只实现了必要的.

Features
------------
* 命令行界面
* 硬盘缓存
* 分块下载
* 自动读取，保存进度
* 自定义http头部
* 不支持Chunked transfer encoding
* 不支持gzip
* JSON-RPC

Sequence diagram
------------
图1  
![](http://uml.mvnsearch.org/github/do-you/test/master/add_uri_1%2Epuml "图1")  
图2  
![](http://uml.mvnsearch.org/github/do-you/test/master/add_uri_2%2Epuml "图2")  

Performance
------------
####**`测试结果仅供娱乐`** 
编译器为mingw32-5.2.0  
测试环境:主机win7 x64 虚拟机win7 x86    
测试过程：
* 虚拟机中用ramdisk建盘存放一个1.45GB的文件，用IIS做http server
* 虚拟机使用NAT模式
* 在主机里下载,均下载到机械硬盘下同一目录
* 测试中DownThemAll，迅雷为单线程下载
* aria2,internet_downloader为4线程
  
说明|任务管理器(1%为80+MB/s)
|---|---|----
右aria2(中间那个小的波峰aria2已经开始了,只是阻塞在写文件)  | ![](https://raw.githubusercontent.com/do-you/internet_downloader/master/picture/3.png "右aria2")
右迅雷  | ![](https://raw.githubusercontent.com/do-you/internet_downloader/master/picture/2.png "右迅雷")
左DownThemAll!  | ![](https://raw.githubusercontent.com/do-you/internet_downloader/master/picture/1.png "左DownThemAll!")
`*internet_downloader并没有做缓存限制，内存最高时500MB(第一次回写时)`
  
Screenshot
------------
编译器为mingw64-5.3.0
![](https://raw.githubusercontent.com/do-you/internet_downloader/master/picture/4.png "YAAW")

Dependency
------------
features | dependency
|---|---|----
json-rpc | nlohmann/json
filesystem | boost

References
------------
* [aria2](https://github.com/tatsuhiro-t/aria2)
* [YAAW](https://github.com/binux/yaaw)

License
------------
GPLv2

Postscript
------------
纯c++的最后一版，下一版会采用Python，c++混合编写一个跨平台的版本，并加入`迅雷离线`支持
