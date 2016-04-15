# internet_downloader
这是一个windows平台上用c++写的基于IOCP的HTTP下载器 

与aria2兼容(只实现了部分参数),aria2:https://github.com/tatsuhiro-t/aria2  
兼容web前端(添加,删除,开始,暂停,显示状态)，YAAW :https://github.com/binux/yaaw  

结构示意图：  
![image](https://github.com/do-you/internet_downloader/raw/master/picture/project_struct.png)  

各个模块的功能：  
#####common目录
>>里面放一些有用的小函数和全局配置  

#####client
>>负责接受网络上的json请求，并解析后提交给json_rpc处理，然后把结果传回去  

#####json_rpc
>>负责监听，每个连接对应一个client对象。调用Internet_Downloader的接口处理请求并把结果以json的格式返回给client  

#####Internet_Downloader
>>代表一个下载器，对外提供接口，对内管理各个下载任务  

#####global_state_cache
>>json_rpc与Internet_Downloader中的一个缓冲层，分类并缓存从Internet_Downloader得到的全局信息以供json_rpc使用  

#####task_schedule
>>Internet_Downloader中的任务调度逻辑组件  

#####down_task
>>代表一个下载任务。  

#####file_ma
>>代表要下载的文件，管理各个block  

#####block
>>代表文件中的空缺块  

#####net_io
>>管理多个connection  

#####connection
>>实际的数据接收者，从网络上接收block指示范围内的数据并写进block里  

#####cache_control
>>实际的数据回写者，记录已缓存数据的数量，并在达到上限值时启动自己的回写线程，回写已缓存数量最大的file_ma中的数据  

#进度

####ver 3.1.0dev  
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

####ver 3.0.0dev
2016-3-2  
架构基本建起来  
平台：Windows  
语言：c++  
依赖的类库：boost  
界面：命令行  
支持的下载协议：HTTP（trunk模式暂未实现）  
实现的特性：磁盘缓存，断点续存，多线程，多任务  
未来的工作：兼容aria2的web前端  


