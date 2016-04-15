# internet_downloader
这是一个windows平台上用c++写的基于IOCP的HTTP下载器 

与aria2兼容(只实现了部分参数),aria2:https://github.com/tatsuhiro-t/aria2  
兼容web前端，YAAW :https://github.com/binux/yaaw  

#进度

####ver 3.1.0
2016-4-10  
updates:  
实现了aria2 json-rpc部分命令  
现在可以用YAAW，添加，删除，开始，暂停，显示状态  
todo:  
完善CMakeLists  
bugs:  
程序里用的是UTF-8编码，但从命令行窗口传进来的字符编码不固定。  
future:  
网络模块跨linux

####ver 3.0.0
2016-3-2  
架构基本建起来  
平台：Windows  
语言：c++  
依赖的类库：boost  
界面：命令行  
支持的下载协议：HTTP（trunk模式暂未实现）  
实现的特性：磁盘缓存，断点续存，多线程，多任务  
未来的工作：兼容aria2的web前端  


